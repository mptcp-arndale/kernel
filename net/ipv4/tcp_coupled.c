/*
 * TCP COUPLED CONGESTION CONTROL:
 *
 * Algorithm: Costin Raiciu, Daemon Wischik, Mark Handley
 * Implementation: Christoph Paasch & Sébastien Barré, UCLouvain (Belgium)
 */

#include <net/tcp.h>
#include <net/mptcp.h>

/* Scaling is done in the numerator with alpha_scale_num and in the denominator
 * with alpha_scale_den.
 *
 * To downscale, we just need to use alpha_scale.
 *
 * We have: alpha_scale = alpha_scale_num / (alpha_scale_den ^ 2)
 */
static int alpha_scale_den = 10;
static int alpha_scale_num = 32;
static int alpha_scale = 12;

struct mptcp_ccc {
	u64 alpha;
};

u32 mptcp_get_crt_cwnd(struct tcp_sock *tp)
{
	struct inet_connection_sock *icsk = inet_csk((struct sock *) tp);

	/* If we are in fast-retransmit, the cwnd is "artificially inflated"
	 * (see RFC5681), thus we use ssthresh as an indication of the cwnd. */
	if (icsk->icsk_ca_state == TCP_CA_Recovery)
		return min(tcp_packets_in_flight(tp), tp->snd_ssthresh);
	else
		return min(tcp_packets_in_flight(tp), tp->snd_cwnd);
}

u32 mptcp_get_total_cwnd(struct multipath_pcb *mpcb)
{
	struct tcp_sock *tp;
	struct sock *sub_sk;
	u32 cwnd = 0;
	tp = mpcb->connection_list;

	mptcp_for_each_sk(mpcb, sub_sk, tp) {
		if (!mptcp_sk_can_send(sub_sk))
			continue;
		cwnd += mptcp_get_crt_cwnd(tp);
	}
	return cwnd;
}

static inline u64 mptcp_get_alpha(struct multipath_pcb *mpcb)
{
	struct mptcp_ccc * mptcp_ccc = inet_csk_ca((struct sock *) mpcb);
	return mptcp_ccc->alpha;
}

static inline void mptcp_set_alpha(struct multipath_pcb *mpcb, u64 alpha)
{
	struct mptcp_ccc * mptcp_ccc = inet_csk_ca((struct sock *) mpcb);
	mptcp_ccc->alpha = alpha;
}

static inline u64 mptcp_ccc_scale(u32 val, int scale)
{
	return (u64) val << scale;
}

static void mptcp_recalc_alpha(struct sock *sk)
{
	struct multipath_pcb *mpcb = mpcb_from_tcpsock(tcp_sk(sk));
	struct tcp_sock *sub_tp;
	struct sock *sub_sk;
	int best_cwnd = 0, best_rtt = 0, tot_cwnd, can_send = 0;
	u64 max_numerator = 0, sum_denominator = 0, alpha = 1;

	if (!mpcb)
		return;

	/* Only one subflow left - fall back to normal reno-behavior
	 * (set alpha to 1) */
	if (mpcb->cnt_established <= 1)
		goto exit;

	/* Do regular alpha-calculation for multiple subflows */

	/* The total congestion window might be zero, if the flighsize is 0 */
	tot_cwnd = mptcp_get_total_cwnd(mpcb);
	if (!tot_cwnd)
		tot_cwnd = 1;

	/* Find the max numerator of the alpha-calculation */
	mptcp_for_each_sk(mpcb, sub_sk, sub_tp) {
		u64 rtt = 1; /* Minimum value is 1, to avoid dividing by 0
			      * u64 - because anyway we later need it */
		u64 tmp;

		if (!mptcp_sk_can_send(sub_sk))
			continue;

		can_send++;

		if (likely(sub_tp->srtt))
			rtt = sub_tp->srtt;
		else
			mptcp_debug("%s: estimated rtt == 0, mpcb_token"
				   ":%d, pi:%d, sub_sk->state:%d\n",
				   __func__, mpcb->mptcp_loc_token,
				   sub_tp->path_index, sub_sk->sk_state);

		/* We need to look for the path, that provides the max-value.
		 * Integer-overflow is not possible here, because
		 * tmp will be in u64.
		 */
		tmp = div64_u64(mptcp_ccc_scale(sub_tp->snd_cwnd,
				alpha_scale_num), rtt * rtt);

		if (tmp >= max_numerator) {
			max_numerator = tmp;
			best_cwnd = sub_tp->snd_cwnd;
			best_rtt = sub_tp->srtt;
		}
	}

	/* No subflow is able to send - we don't care anymore */
	if (unlikely(!can_send))
		goto exit;

	/* Calculate the denominator */
	mptcp_for_each_sk(mpcb, sub_sk, sub_tp) {
		u64 rtt = 1; /* Minimum value is 1, to avoid dividing by 0
			      * u64 - because anyway we later need it */

		if (!mptcp_sk_can_send(sub_sk))
			continue;

		if (likely(sub_tp->srtt))
			rtt = sub_tp->srtt;
		else
			mptcp_debug("%s: estimated rtt == 0, mpcb_token"
				   ":%d, pi:%d, sub_sk->state:%d\n",
				   __func__, mpcb->mptcp_loc_token,
				   sub_tp->path_index, sub_sk->sk_state);

		sum_denominator += div_u64(
				mptcp_ccc_scale(sub_tp->snd_cwnd,
						alpha_scale_den) * best_rtt,
						rtt);
	}
	sum_denominator *= sum_denominator;
	if (unlikely(!sum_denominator)) {
		mptcp_debug("%s: sum_denominator == 0, cnt_established:%d\n",
				__func__, mpcb->cnt_established);
		mptcp_for_each_sk(mpcb, sub_sk, sub_tp)
			mptcp_debug("%s: pi:%d, state:%d\n, rtt:%u, cwnd: %u",
					__func__, sub_tp->path_index,
					sub_sk->sk_state, sub_tp->srtt,
					sub_tp->snd_cwnd);
	}

	alpha = div64_u64(mptcp_ccc_scale(tot_cwnd, alpha_scale_num) *
					best_cwnd, sum_denominator);

	if (unlikely(!alpha))
		alpha = 1;

exit:
	mptcp_set_alpha(mpcb, alpha);
}

static void mptcp_cc_init(struct sock *sk)
{
	if (mpcb_from_tcpsock(tcp_sk(sk)))
		mptcp_set_alpha(mpcb_from_tcpsock(tcp_sk(sk)), 1);
	/* If we do not mptcp, behave like reno: return */
}

static void mptcp_cwnd_event(struct sock *sk, enum tcp_ca_event event)
{
	if (event == CA_EVENT_LOSS)
		mptcp_recalc_alpha(sk);
}

static void mptcp_fc_cong_avoid(struct sock *sk, u32 ack, u32 in_flight)
{
	struct tcp_sock *tp = tcp_sk(sk), *meta_tp;
	struct multipath_pcb *mpcb = mpcb_from_tcpsock(tp);
	int snd_cwnd;

	if (!mpcb || !tp->mpc) {
		tcp_reno_cong_avoid(sk, ack, in_flight);
		return;
	}

	meta_tp = mpcb_meta_tp(mpcb);

	if (!tcp_is_cwnd_limited(sk, in_flight))
		return;

	if (tp->snd_cwnd <= tp->snd_ssthresh) {
		/* In "safe" area, increase. */
		tcp_slow_start(tp);
		mptcp_recalc_alpha(sk);
		return;
	}

	if (mpcb->cnt_established > 1) {
		u64 alpha = mptcp_get_alpha(mpcb);

		/* This may happen, if at the initialization, the mpcb
		 * was not yet attached to the sock, and thus
		 * initializing alpha failed.
		 */
		if (unlikely(!alpha))
			alpha = 1;

		snd_cwnd = mptcp_get_total_cwnd(mpcb);

		snd_cwnd = (int) div_u64 ((u64) mptcp_ccc_scale(snd_cwnd,
						alpha_scale), alpha);

		/* snd_cwnd_cnt >= max (scale * tot_cwnd / alpha, cwnd)
		 * Thus, we select here the max value. */
		if (snd_cwnd < tp->snd_cwnd)
			snd_cwnd = tp->snd_cwnd;
	} else {
		snd_cwnd = tp->snd_cwnd;
	}

	if (sysctl_tcp_abc) {
		if (tp->bytes_acked >= snd_cwnd * meta_tp->mss_cache) {
			/* Only a single mss for all subflows - thus use the
			 * one of the meta-tp */
			tp->bytes_acked -= snd_cwnd * meta_tp->mss_cache;
			if (tp->snd_cwnd < tp->snd_cwnd_clamp)
				tp->snd_cwnd++;
		}
	} else {
		if (tp->snd_cwnd_cnt >= snd_cwnd) {
			if (tp->snd_cwnd < tp->snd_cwnd_clamp) {
				tp->snd_cwnd++;
				mptcp_recalc_alpha(sk);
			}

			tp->snd_cwnd_cnt = 0;
		} else {
			tp->snd_cwnd_cnt++;
		}
	}
}

static struct tcp_congestion_ops mptcp_fc = {
	.init		= mptcp_cc_init,
	.ssthresh	= tcp_reno_ssthresh,
	.cong_avoid	= mptcp_fc_cong_avoid,
	.cwnd_event	= mptcp_cwnd_event,
	.min_cwnd	= tcp_reno_min_cwnd,
	.owner		= THIS_MODULE,
	.name		= "coupled",
};

static int __init mptcp_fc_register(void)
{
	BUILD_BUG_ON(sizeof(struct mptcp_ccc) > ICSK_CA_PRIV_SIZE);
	return tcp_register_congestion_control(&mptcp_fc);
}

static void __exit mptcp_fc_unregister(void)
{
	tcp_unregister_congestion_control(&mptcp_fc);
}

module_init(mptcp_fc_register);
module_exit(mptcp_fc_unregister);

MODULE_AUTHOR("Christoph Paasch, Sébastien Barré");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MPTCP COUPLED CONGESTION CONTROL");
MODULE_VERSION("0.1");