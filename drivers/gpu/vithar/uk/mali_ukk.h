/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2010-2011 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_ukk.h
 * Types and definitions that are common across OSs for the kernel side of the
 * User-Kernel interface.
 */

#ifndef _UKK_H_
#define _UKK_H_

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#include <osk/mali_osk.h>
#include <malisw/mali_stdtypes.h>
#include <uk/mali_uk.h>
#include <plat/mali_uk_os.h>

/**
 * Incomplete definitions of ukk_session, ukk_call_context to satisfy header file dependency in plat/mali_ukk_os.h
 */
typedef struct ukk_session ukk_session;
typedef struct ukk_call_context ukk_call_context;
#include <plat/mali_ukk_os.h> /* needed for ukkp_session definition */

/**
 * @addtogroup uk_api User-Kernel Interface API
 * @{
 */

/**
 * @addtogroup uk_api_kernel UKK (Kernel side)
 * @{
 *
 * A kernel-side device driver implements the UK interface with the help of the UKK. The UKK
 * is an OS independent API for kernel-side code to accept requests from the user-side to 
 * execute functions in the kernel-side device driver.
 *
 * A few definitions:
 * - the kernel-side device driver implementing the UK interface is called the UKK client driver
 * - the user-side library, application, or driver communicating with the UKK client driver is called the UKU client driver
 * - the UKK API implementation is called the UKK core. The UKK core is linked with your UKK client driver.
 *
 * When a UKK client driver starts it needs to initialize the UKK core by calling ukk_start() and
 * similarly ukk_stop() when the UKK client driver terminates. A UKK client driver is normally
 * started by an operating system when a device boots.
 *
 * A UKU client driver provides services that are implemented either completely in user-space, kernel-space, or
 * a combination of both. A UKU client driver makes UK calls to its UKK client driver to execute any functionality
 * of services that is implemented in kernel-space. 
 *
 * To make a UK call the UKU client driver needs to establish a connection with the UKK client driver. The UKU API 
 * provides the uku_open() call to establish this connection. Normally this results in the OS calling the open
 * entry point of the UKK client driver. Here, the UKK client driver needs to initialize a UKK session object
 * with ukk_session_init() to represent this connection, and register a function that will execute the UK calls 
 * requested over this connection (or UKK session). This function is called the UKK client dispatch handler. 
 *
 * To prevent the UKU client driver executing an incompatible UK call implementation, the UKK session object  
 * stores the version of the UK calls supported by the function registered to execute the UK calls. As soon as the 
 * UKU client driver established a connection with the UKK client driver, uku_open() makes an internal UK call to 
 * request the version of the UK calls supported by the UKK client driver and will fail if the version expected
 * by the UKU client driver is not compatible with the version supported by the UKK client driver. Internal UK calls
 * are handled by the UKK core itself and don't reach the UKK client dispatch handler.
 *
 * Once the UKU client driver has established a (compatible) connection with the UKK client driver, the UKU
 * client driver can execute UK calls by using uku_call(). This normally results in the OS calling the ioctl 
 * handler of your UKK client driver and presenting it with the UK call argument structure that was passed
 * to uku_call(). It is the responsibility of the ioctl handler to copy the UK call argument structure from
 * user-space to kernel-space and provide it to the UKK dispatch function, ukk_dispatch(), for execution. Depending
 * on the particular UK call, the UKK dispatch function will either call the UKK client dispatch handler associated
 * with the UKK session, or the UKK core dispatch handler if it is an UK internal call. When the UKK dispatch
 * function returns, the return code of the UK call and the output and input/output parameters in the UK call argument
 * structure will have been updated. Again, it is the responsibility of the ioctl handler to copy the updated
 * UK call argument structure from kernel-space back to user-space.
 *
 * When the UKK client dispatch handler is called it is passed the UK call argument structure (along with a
 * UK call context which is discussed later). The UKK client dispatch handler uses the uk_header structure in the
 * UK call argument structure (which is always the first member in this structure) to determine which UK call in
 * particular needs to be executed. The uk_header structure is a union of a 32-bit number containing the UK call 
 * function number (as defined by the UKK client driver) and a mali_error return value that will store the return
 * value of the UK call. The 32-bit UK call function number is normally used to select a particular case in a switch
 * statement that implements the particular UK call which finally stores the result of the UK call in the mali_error
 * return value of the uk_header structure.
 *
 * A UK call implementation is provided with access to a number of objects it may need during the UK call through
 * a UKK call context. This UKK call context contains
 *   - a pointer to the UKK session for the UK call
 *   - a thread context from which the UK call was made (OS specific)
 *   - a general purpose data field (large enough to contain a pointer)
 *
 * It is the responsibility of the ioctl handler to initialize a UKK call context using ukk_call_prepare() and pass
 * it on to the UKK dispatch function. The UK call implementation then uses ukk_session_get(), ukk_thread_ctx_get()
 * and ukk_call_data_get() to retrieve the stored objects in the UKK call context. The UK call implementation 
 * normally uses the UKK session pointer returned from ukk_session_get() to access the UKK client driver's context
 * in which the UKK session is embedded. For example:
 *	struct kbase_context {
 *          int some_kbase_context_data;
 *          int more_kbase_context_data;
 *          ukk_session ukk_session_member;
 *      } *kctx;
 *	kctx = CONTAINER_OF(ukk_session_get(ukk_call_ctx), kbase_context, ukk_session_member);
 *
 * A UK call may use an argument structure with embedded pointers pointing to user-space memory. To support 
 * an OS independent implemenation of the UK calls, the UKK API provides the OS independent ukk_buffer_open(), 
 * ukk_buffer_close() and ukk_buffer_get() functions to access the user-space memory areas in kernel-space. 
 *
 * All of this can be translated into the following minimal sample code for a UKK client driver:
@code
// Sample code for an imaginary UKK client driver 'TESTDRV' implementing the 'TESTDRV_UK_FOO_FUNC' UK call
//
#define TESTDRV_VERSION_MAJOR 0
#define TESTDRV_VERSION_MINOR 1

typedef enum testdrv_uk_function
{
	TESTDRV_UK_FOO_FUNC = (UK_FUNC_ID + 0)
} testdrv_uk_function;

typedef struct testdrv_uk_foo_args
{
	uk_header header; 
	int counters[10];      // input
	int prev_counters[10]; // output
} testdrv_uk_foo_args;

typedef struct testdrv_session
{
	int counters[10];
	ukk_session ukk_session_obj;
} testdrv_session;

testdrv_open(os_driver_context *osctx)
{
	testdrv_session *ts;
	ts = osk_malloc(sizeof(*ts));
	ukk_session_init(&ts->ukk_session_obj, testdrv_ukk_dispatch, TESTDRV_VERSION_MAJOR, TESTDRV_VERSION_MINOR);
	osctx->some_field = ts;
}
testdrv_close(os_driver_context *osctx)
{
	testdrv_session *ts = osctx->some_field;
	ukk_session_term(&ts->ukk_session_obj)
	osk_free(ts);
	osctx->some_field = NULL;
}
testdrv_ioctl(os_driver_context *osctx, void *user_arg, u32 args_size)
{
	testdrv_session *ts = osctx->some_field;
	ukk_call_context call_ctx;
	void *kernel_arg;
	
	kernel_arg = os_copy_to_kernel_space(user_arg, args_size);
	
	ukk_call_prepare(&call_ctx, &ts->ukk_session_obj, NULL);
	
	ukk_dispatch(&call_ctx, kernel_arg, args_size);
	
	os_copy_to_user_space(user_arg, kernel_arg, args_size);
}
mali_error testdrv_ukk_dispatch(ukk_call_context *call_ctx, void *arg, u32 args_size)
{
	uk_header *header = arg;
	mali_error ret = MALI_ERROR_FUNCTION_FAILED;

	switch(header->id)
	{
		case TESTDRV_UK_FOO_FUNC:
		{
			testdrv_uk_foo_args *foo_args = arg;
			if (sizeof(*foo_args) == args_size)
			{
				mali_error result;
				result = foo(call_ctx, foo_args);
				header->ret = result;
				ret = MALI_ERROR_NONE;
			}
			break;
		}
	}
	return ret;
}
mali_error foo(ukk_call_context *call_ctx, testdrv_uk_foo_args *args) {
	// foo updates the counters in the testdrv_session object and returns the old counters
	testdrv_session *session = CONTAINER_OF(ukk_session_get(call_ctx), testdrv_session, ukk_session_obj);
	memcpy(&args->prev_counters,  session->counters, 10 * sizeof(int));
	memcpy(&session->counters, &args->counters, 10 * sizeof(int));
	return MALI_ERROR_NONE;
}
@endcode
*/

/**
 * Maximum size of UK call argument structure supported by UKK clients
 */
#define UKK_CALL_MAX_SIZE 128

/**
 * @brief Dispatch callback of UKK client
 * 
 * The UKK client's dispatch function is called by UKK core ukk_dispatch()
 *
 * The UKK client's dispatch function should return MALI_ERROR_NONE when it
 * has accepted and executed the UK call. If the UK call is not recognized it
 * should return MALI_ERROR_FUNCTION_FAILED.
 *
 * An example of a piece of code from a UKK client dispatch function:
 * @code
 * uk_header *header = (uk_header *)arg;
 * switch(header->id) {
 *     case MYCLIENT_FUNCTION: { 
 *       if (args_size != sizeof(myclient_function_args)) {
 *          return MALI_ERROR_FUNCTION_FAILED; // argument structure size mismatch
 *       } else {
 *          // execute UK call and store result back in header
 *          header->ret = do_my_client_function(ukk_ctx, args);
 *          return MALI_ERROR_NONE;
 *       }
 *     default:
 *         return MALI_ERROR_FUNCTION_FAILED; // UK call function number not recognized
 * }
 * @endcode
 *
 * For details, see ukk_dispatch().
 *
 * Debug builds will assert when a NULL pointer is passed for ukk_ctx or args, or,
 * args_size < sizeof(uk_header).
 *
 * @param[in] ukk_ctx     Pointer to a call context
 * @param[in,out] args    Pointer to a argument structure of a UK call
 * @param[in] args_size   Size of the argument structure (in bytes)
 * @return MALI_ERROR_NONE on success. MALI_ERROR_FUNCTION_FAILED when the UK call was not recognized.
 */
typedef mali_error (*ukk_dispatch_function)(ukk_call_context * const ukk_ctx, void * const args, u32 args_size);

/**
 * Driver session related data for the UKK core. 
 */
struct ukk_session
{
	/**
	 * Session data stored by the OS specific implementation of the UKK core
	 */
	ukkp_session internal_session;

	/**
	 * UKK client version supported by the call backs provided below - major number of version
	 */
	u16 version_major;

	/**
	 * UKK client version supported by the call backs provided below - minor number of version
	 */
	u16 version_minor;

	/**
	 * Function in UKK client that executes UK calls for this UKK session, see
	 * ukk_dispatch_function.
	 */
	ukk_dispatch_function dispatch;
};

/**
 * Stucture containing context data passed in to each UK call. Before each UK call it is initialized
 * by the ukk_call_prepare() function. UK calls can retrieve the context data using the functions 
 * ukk_session_get(), ukk_thread_ctx_get(), ukk_call_data_get() and ukk_session_data_get().
 */
struct ukk_call_context
{
	/**
	 * Pointer to UKK core session data. 
	 * Pass to ukk_session_data_get() to access the session data of the UKK client.
	 */
	ukk_session *ukk_session;
	/**
	 * Pointer to the caller thread context that initiated the UK call.
	 */
	void *thread_ctx;
	/**
	 * General purpose data value (large enough to store a pointer) to pass along to the function
	 * implementing the UK call in the UKK client.
	 */
	uintptr_t data;
};

/**
 * Values describing the type of access to the buffer created to access a memory area in user space.
 * The value can be used to identify access to a memory area in user space as input only, output only, or both
 * input and output. The values cannot be combined. Use with the ukk_buffer_open() and
 * ukk_buffer_close() functions that deal with accessing embedded pointers in the argument structure
 * of a UK call.
 */
typedef enum ukk_buffer_type
{
	/**
	 * The kernel space buffer is for input purposes only. Existing data from a user space memory area
	 * will be copied to the kernel space buffer when the kernel space buffer is opened.
	 */
	UKK_BUFFER_INPUT,
	/**
	 * The kernel space buffer is for output purposes only. The data from the kernel space buffer will
	 * be copied back to a user space memory area when the kernel space buffer is closed.
	 */
	UKK_BUFFER_OUTPUT,
	/**
	 * The kernel space buffer is for input and output purposes. Existing data from a user space memory area
	 * will be copied to the kernel space buffer when the kernel space buffer is opened. The data from 
	 * the kernel space buffer will be copied back to a user space memory area when the kernel space 
	 * buffer is closed.
	 */
	UKK_BUFFER_INPUT_OUTPUT
} ukk_buffer_type;

/**
 * A ukk_buffer object represents a memory area in user space that is made accessible in kernel space.
 * This object is managed by the ukk_buffer_open(), ukk_buffer_close(), and ukk_buffer_get() functions.
 */
typedef struct ukk_buffer
{
	/**
	 * Pointer to the kernel space buffer created to buffer the user space memory area.
	 */
	void *kernel_buffer;
	/**
	 * Pointer to the user space memory area passed in to ukk_buffer_open()
	 */
	void *user_buffer;
	/**
	 * Specifies the buffer type (input, output or both) as passed in to ukk_buffer_open()
	 */
	ukk_buffer_type type;
	/**
	 * Specifies the size of the user space memory area as passed in to ukk_buffer_open()
	 */
	u32 size;
} ukk_buffer;

/**
 * @brief UKK core startup
 *
 * Must be called during the UKK client driver initialization before accessing any UKK provided functionality.
 *
 * @return MALI_ERROR_NONE on success. Any other value indicates failure.
 */
mali_error ukk_start(void);

/**
 * @brief UKK core shutdown
 * 
 * Must be called during the UKK client driver termination to free any resources UKK might have allocated.
 *
 * After this has been called no UKK functionality may be accessed.
 */
void ukk_stop(void);

/**
 * @brief Initialize a UKK session
 * 
 * When a UKK client driver is opened, a UKK session object needs to be initialized to
 * store information specific to that session with the UKK client driver. 
 *
 * This UKK session object is normally contained in a session specific data structure created 
 * by the OS specific open entry point of the UKK client driver. The entry point of the
 * UKK client driver that receives requests from user space to execute UK calls will
 * need to pass on a pointer to this UKK session object to the ukk_dispatch() function to
 * execute a UK call for the active session.
 *
 * A UKK session supports executing UK calls for a particular version of the UKK client driver
 * interface. A pointer to the dispatch function that will execute the UK calls needs to
 * be passed to ukk_session_init(), along with the version (major and minor) of the UKK client
 * driver interface that this dispatch function supports.
 *
 * When the UKK client driver is closed, the initialized UKK session object needs to be
 * terminated. See ukk_session_term().
 *
 * Debug builds will assert when a NULL pointer is passed for ukk_ctx or dispatch.
 *
 * @param[out] ukk_session  Pointer to UKK session to initialize
 * @param[in] dispatch      Pointer to dispatch function to associate with the UKK session
 * @param[in] version_major Dispatch function will handle UK calls for this major version
 * @param[in] version_minor Dispatch function will handle UK calls for this minor version
 * @return MALI_ERROR_NONE on success. Any other value indicates failure. 
 */
mali_error ukk_session_init(ukk_session *ukk_session, ukk_dispatch_function dispatch, u16 version_major, u16 version_minor);

/**
 * @brief Terminates a UKK session
 * 
 * Frees any resources allocated for the UKK session object. No UK calls for this session
 * may be executing when calling this function. This function invalidates the UKK session 
 * object and must not be used anymore until it is initialized again with ukk_session_init().
 * 
 * Debug builds will assert when a NULL pointer is passed for ukk_session.
 *
 * @param[in,out] ukk_session  Pointer to UKK session to terminate
 */
void ukk_session_term(ukk_session *ukk_session);

/**
 * @brief Prepare a context in which to execute a UK call
 * 
 * UK calls are passed a call context that allows them to get access to the UKK session data, 
 * the UKK client session data, the calling thread's context and a general purpose value that may
 * be necessary for the implementation of the UK call.
 *
 * The ukk_call_prepare() function initializes this call context with the general purpose value
 * set to 0. The general purpose value may be set later using the ukk_call_data_set() function.
 *
 * Given a call context, UK calls use ukk_session_get() to get access to the UKK session data,
 * ukk_thread_ctx_get() to get access to the calling thread's context and ukk_call_data_get() to
 * get access to a uintptr_t sized value of general purpose data. With the UKK session data
 * you get access to the UKK client session data using the ukk_session_data_get() function.
 * 
 * Debug builds will assert when a NULL pointer is passed for ukk_ctx, ukk_session. A NULL
 * thread_ctx is allowed if the OS-specific UKK implemenation doesn't require any reference
 * to the calling thread's context.
 *
 * @param[out] ukk_ctx     Pointer to call context to initialize.
 * @param[in] ukk_session  Pointer to UKK session to associate with the call context
 * @param[in] thread_ctx   Pointer to the calling thread's context to associate with the call context
 */
void ukk_call_prepare(ukk_call_context * const ukk_ctx, ukk_session * const ukk_session, void * const thread_ctx);

/**
 * @brief Get the UKK session of a call context
 * 
 * Returns the UKK session associated with a call context. See ukk_call_prepare.
 *
 * Debug builds will assert when a NULL pointer is passed for ukk_ctx.
 *
 * @param[in] ukk_ctx Pointer to call context
 * @return Pointer to UKK session associated with the call context
 */
void *ukk_session_get(ukk_call_context * const ukk_ctx);

/**
 * @brief Set the data value of a call context
 * 
 * Sets the data value associated with a call context. See ukk_call_prepare and ukk_call_data_get.
 *
 * Debug builds will assert when a NULL pointer is passed for ukk_ctx.
 *
 * @param[in,out] ukk_ctx Pointer to a call context
 * @param[in] data        data value (large enough to store a pointer) to associate with a call context
 */
void ukk_call_data_set(ukk_call_context * const ukk_ctx, uintptr_t data);

/**
 * @brief Get the data value of a call context
 * 
 * Returns the data value associated with a call context. See ukk_call_prepare and ukk_call_data_set.
 *
 * Debug builds will assert when a NULL pointer is passed for ukk_ctx.
 *
 * @param[in] ukk_ctx Pointer to a call context
 * @return data value associated with the call context (large enough to store a pointer)
 */
uintptr_t ukk_call_data_get(ukk_call_context * const ukk_ctx);

/**
 * @brief Get the thread context of a call context
 *
 * Returns the calling thread's context associated with a call context. See ukk_call_prepare.
 *
 * Debug builds will assert when a NULL pointer is passed for ukk_ctx.
 *
 * @param[in] ukk_ctx Pointer to a call context
 * @return Pointer to the calling thread's context associated with the call context
 */
void *ukk_thread_ctx_get(ukk_call_context * const ukk_ctx);

/**
 * @brief Open a buffer for accessing or updating a user space memory area 
 * 
 * Used for accessing user space pointers in the argument structure of a UK call. The argument
 * structure of a UK call should be defined such that it contains a field specifying the size
 * of the memory area pointed to by the user space pointer, and serves as the size argument of
 * this function.
 *
 * A transient buffer in kernel space will be created to access the memory area pointed to by the
 * user space pointer. The type argument specifies the type of access (input, output, or
 * input and output).
 *
 * When the type is UKK_BUFFER_INPUT or UKK_BUFFER_INPUT_OUTPUT, the transient buffer
 * will be initialized with the data from the memory area in user space. 
 *
 * When the type is UKK_BUFFER_OUTPUT the transient buffer will not be initialized and contain
 * random data. The user must ensure the entire buffer is written when the buffer is
 * closed to prevent information leaks from kernel to user space.
 *
 * Note that a buffer should be closed before returning from a UK call. Leaving buffers allocated
 * between UK calls is not allowed.
 *
 * Changes made to the data in a buffer are not visible to user space until you close the buffer.
 *
 * Debug builds will assert when a NULL pointer is passed for buffer, user_buffer, size is zero
 * or an unknown enumerated value is passed for type.
 *
 * @param[out] buffer      Pointer to ukk_buffer that ukk_buffer_open will initialize.
 * @param[in] user_buffer  Pointer to memory area in user space
 * @param[in] size         Size of the memory area in user space (in bytes)
 * @param[in] type         Flags describing type of access to the memory area. See ukk_buffer_type.
 * @return Pointer to kernel accessible buffer containing the data from the memory area in user space. NULL on failure.
 */
mali_error ukk_buffer_open(ukk_buffer * const buffer, void * const user_buffer, size_t size, ukk_buffer_type type);

/**
 * @brief Close a buffer opened for accessing or updating a user-side memory area 
 * 
 * Releases any resources created when the buffer was opened.
 *
 * If the buffer was opened for output, or input and output, any changes in the buffer will be copied 
 * to the user space memory area. If an error occurs, this function will return MALI_ERROR_FUNCTION_FAILED
 * and the kernel space buffer will be released without having copied the data (completely) to the
 * user space memory area.
 *
 * Debug builds will assert when a NULL pointer is passed for buffer, or, the internal data of the
 * ukk_buffer object got corrupted.
 *
 * @param[in,out] buffer  Pointer to buffer returned from ukk_buffer_open.
 * @return MALI_ERROR_NONE on success. MALI_ERROR_FUNCTION_FAILED when the buffer couldn't
 * be copied back completely to the user space memory area.
 */
mali_error ukk_buffer_close(ukk_buffer * const buffer);

/**
 * @brief Retrieves the kernel space pointer of the buffer
 * 
 * Returns the kernel space pointer of the buffer. This kernel space pointer is only valid
 * until the buffer is closed.
 *
 * Calling ukk_buffer_get() on a buffer that failed to open, will return NULL.
 * Calling ukk_buffer_get() on a buffer that hasn't been opened yet returns random data.
 *
 * Debug builds will assert when a NULL pointer is passed for buffer.
 *
 * @param[in] buffer  Pointer to buffer returned from ukk_buffer_open.
 * @return Kernel space pointer of the buffer. NULL when used on a buffer that failed to open.
 */
static INLINE void *ukk_buffer_get(ukk_buffer * const buffer)
{
	OSK_ASSERT(NULL != buffer);
	return buffer->kernel_buffer;
}

/**
 * @brief Dispatch a UK call
 * 
 * Dispatches the UK call to the UKK client or the UKK core in case of an internal UK call. The id field
 * in the header field of the argument structure identifies which UK call needs to be executed. Any
 * UK call with id equal or larger than UK_FUNC_ID is dispatched to the UKK client.
 *
 * If the UK call was accepted by the dispatch handler of the UKK client or UKK core, this function returns
 * with MALI_ERROR_NONE and the result of executing the UK call is stored in the header.ret field of the
 * in the argument structure. This function returns MALI_ERROR_FUNCTION_FAILED when the UK call is not
 * accepted by the dispatch handler.
 *
 * If a UK call fails while executing in the dispatch handler of the UKK client or UKK core
 * the UK call is reponsible for cleaning up any resources it allocated up to the point a failure
 * occurred.
 *
 * Before accepting a UK call, the dispatch handler of the UKK client or UKK core should compare the
 * the size of the argument structure based on the function id in header.id with the args_size parameter.
 * Only if they match the UK call should be attempted, otherwise MALI_ERROR_FUNCTION_FAILED
 * should be returned.
 *
 * An example of a piece of code from a UKK client dispatch handler:
 * @code
 * uk_header *header = (uk_header *)arg;
 * switch(header->id) {
 *     case MYCLIENT_FUNCTION: { 
 *       if (args_size != sizeof(myclient_function_args)) {
 *          return MALI_ERROR_FUNCTION_FAILED; // argument structure size mismatch
 *       } else {
 *          // execute UK call and store result back in header
 *          header->ret = do_my_client_function(ukk_ctx, args);
 *          return MALI_ERROR_NONE;
 *       }
 *     default:
 *         return MALI_ERROR_FUNCTION_FAILED; // UK call function number not recognized
 * }
 * @endcode
 *
 * Debug builds will assert when a NULL pointer is passed for ukk_ctx, args or args_size 
 * is < sizeof(uk_header).
 *
 * @param[in] ukk_ctx     Pointer to a call context
 * @param[in,out] args    Pointer to a argument structure of a UK call
 * @param[in] args_size   Size of the argument structure (in bytes)
 * @return MALI_ERROR_NONE on success. MALI_ERROR_FUNCTION_FAILED when the UK call was not accepted
 * by the dispatch handler of the UKK client or UKK core, or the passed in argument structure
 * is not large enough to store the required uk_header structure.
 */
mali_error ukk_dispatch(ukk_call_context * const ukk_ctx, void * const args, u32 args_size);

/** @} end group uk_api_kernel */

/** @} end group uk_api */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _UKK_H_ */