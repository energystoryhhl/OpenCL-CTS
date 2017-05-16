//
// Copyright (c) 2017 The Khronos Group Inc.
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#include "cl_utils.h"
#include <stdlib.h>

#if !defined (_WIN32)
#include <sys/mman.h>
#endif

#include <math.h>

#if !defined (_WIN32)
#include <stdint.h>
#endif

#include "test_config.h"
#include "string.h"

#define HALF_MIN 1.0p-14


const char *vector_size_name_extensions[kVectorSizeCount+kStrangeVectorSizeCount] = { "", "2", "4", "8", "16", "3" };
const char *vector_size_strings[kVectorSizeCount+kStrangeVectorSizeCount] = { "1", "2", "4", "8", "16", "3" };
const char *align_divisors[kVectorSizeCount+kStrangeVectorSizeCount] = { "1", "2", "4", "8", "16", "4" };
const char *align_types[kVectorSizeCount+kStrangeVectorSizeCount] = { "half", "int", "long", "long2", "long4", "long" };


void            *gIn_half = NULL;
void            *gOut_half = NULL;
void            *gOut_half_reference = NULL;
void            *gOut_half_reference_double = NULL;
void            *gIn_single = NULL;
void            *gOut_single = NULL;
void            *gOut_single_reference = NULL;
void            *gIn_double = NULL;
void            *gOut_double = NULL;
void            *gOut_double_reference = NULL;
cl_mem          gInBuffer_half = NULL;
cl_mem          gOutBuffer_half = NULL;
cl_mem          gInBuffer_single = NULL;
cl_mem          gOutBuffer_single = NULL;
cl_mem          gInBuffer_double = NULL;
cl_mem          gOutBuffer_double = NULL;

cl_device_type  gDeviceType = CL_DEVICE_TYPE_DEFAULT;
cl_device_id    gDevice = NULL;
cl_context       gContext = NULL;
cl_command_queue      gQueue = NULL;
uint32_t        gDeviceFrequency = 0;
uint32_t        gComputeDevices = 0;
size_t          gMaxThreadGroupSize = 0;
size_t          gWorkGroupSize = 0;
int             gTestCount = 0;
int             gFailCount = 0;
bool            gWimpyMode = false;
int             gTestDouble = 0;
uint32_t		gDeviceIndex = 0;

#if defined( __APPLE__ )
int             gReportTimes = 1;
#else
int             gReportTimes = 0;
#endif

#pragma mark -

static void CL_CALLBACK notify_callback(const char *errinfo, const void *private_info, size_t cb, void *user_data)
{
    vlog( "%s\n", errinfo );
}

int InitCL( void )
{
    cl_platform_id platform = NULL;
    size_t configSize = sizeof( gComputeDevices );
    int error;
    
    if( (error = clGetPlatformIDs(1, &platform, NULL) ) )
        return error;
    
	// gDeviceType & gDeviceIndex are globals set in ParseArgs
	
	cl_uint ndevices;
	if ( (error = clGetDeviceIDs(platform, gDeviceType, 0, NULL, &ndevices)) )
		return error;
		
    cl_device_id *gDeviceList = (cl_device_id *)malloc(ndevices*sizeof( cl_device_id ));
    if ( gDeviceList == 0 )
    {
        log_error("Unable to allocate memory for devices\n");
        return -1;
    }
    if( (error = clGetDeviceIDs(platform,  gDeviceType, ndevices, gDeviceList, NULL )) )
    {
        free( gDeviceList );
        return error;
    }
  
    gDevice = gDeviceList[gDeviceIndex];
    free( gDeviceList );
    
#if MULTITHREAD
    if( (error = clGetDeviceInfo( gDevice, CL_DEVICE_MAX_COMPUTE_UNITS,  configSize, &gComputeDevices, NULL )) )
#endif
	gComputeDevices = 1;
    
    configSize = sizeof( gMaxThreadGroupSize );
    if( (error = clGetDeviceInfo( gDevice, CL_DEVICE_MAX_WORK_GROUP_SIZE, configSize, &gMaxThreadGroupSize,  NULL )) )
        gMaxThreadGroupSize = 1;
    
    // Use only one-eighth the work group size
    if (gMaxThreadGroupSize > 8)
        gWorkGroupSize = gMaxThreadGroupSize / 8;
    else
        gWorkGroupSize = gMaxThreadGroupSize;
    
    configSize = sizeof( gDeviceFrequency );
    if( (error = clGetDeviceInfo( gDevice, CL_DEVICE_MAX_CLOCK_FREQUENCY, configSize, &gDeviceFrequency,  NULL )) )
        gDeviceFrequency = 1;
    
    // Check extensions
    size_t extSize = 0;
    int hasDouble = 0;
    if((error = clGetDeviceInfo( gDevice, CL_DEVICE_EXTENSIONS, 0, NULL, &extSize)))
    {   vlog_error( "Unable to get device extension string to see if double present. (%d) \n", error ); }
    else
    {
        char *ext = (char *)malloc( extSize );
        if( NULL == ext )
        { vlog_error( "malloc failed at %s:%d\nUnable to determine if double present.\n", __FILE__, __LINE__ ); }
        else
        {
            if((error = clGetDeviceInfo( gDevice, CL_DEVICE_EXTENSIONS, extSize, ext, NULL)))
            {    vlog_error( "Unable to get device extension string to see if double present. (%d) \n", error ); }
            else
            {
                if( strstr( ext, "cl_khr_fp64" ))
                    hasDouble = 1;
            }
            free(ext);
        }
    }
    gTestDouble ^= hasDouble;
    
    vlog( "%d compute devices at %f GHz\n", gComputeDevices, (double) gDeviceFrequency / 1000. );
    vlog( "Max thread group size is %lld.\n", (uint64_t) gMaxThreadGroupSize );
    
    gContext = clCreateContext( NULL, 1, &gDevice, notify_callback, NULL, &error );
    if( NULL == gContext )
    {
        vlog_error( "clCreateDeviceGroup failed. (%d)\n", error );
        return -1;
    }
    
    gQueue = clCreateCommandQueue(gContext, gDevice, 0, &error);
    if( NULL == gQueue )
    {
        vlog_error( "clCreateContext failed. (%d)\n", error );
        return -2;
    }
    
#if defined( __APPLE__ )
    // FIXME: use clProtectedArray
#endif
    //Allocate buffers
    gIn_half   = malloc( getBufferSize(gDevice)/2  );
    gOut_half = malloc( getBufferSize(gDevice)/2  );
    gOut_half_reference = malloc( getBufferSize(gDevice)/2  );
    gOut_half_reference_double = malloc( getBufferSize(gDevice)/2  );
    gIn_single   = malloc( getBufferSize(gDevice)  );
    gOut_single = malloc( getBufferSize(gDevice)  );
    gOut_single_reference = malloc( getBufferSize(gDevice)  );
    gIn_double   = malloc( (2*getBufferSize(gDevice))  );
    gOut_double = malloc( (2*getBufferSize(gDevice))  );
    gOut_double_reference = malloc( (2*getBufferSize(gDevice))  );
    
    if
        ( 
         NULL == gIn_half || NULL == gOut_half || NULL == gOut_half_reference    || NULL == gOut_half_reference_double ||
         NULL == gIn_single || NULL == gOut_single || NULL == gOut_single_reference ||
         NULL == gIn_double || NULL == gOut_double || NULL == gOut_double_reference
         )
        return -3;
    
    gInBuffer_half = clCreateBuffer(gContext, CL_MEM_READ_ONLY, getBufferSize(gDevice), NULL, &error);
    if( gInBuffer_half == NULL )
    {
        vlog_error( "clCreateArray failed for input (%d)\n", error );
        return -4;
    }
    
    gInBuffer_single = clCreateBuffer( gContext, 
                                      CL_MEM_READ_ONLY, 
                                      getBufferSize(gDevice),
                                      NULL,
                                      &error );
    if( gInBuffer_single == NULL )
    {
        vlog_error( "clCreateArray failed for input (%d)\n", error );
        return -4;
    }
    
    gInBuffer_double = clCreateBuffer( gContext, 
                                      CL_MEM_READ_ONLY, 
                                      (size_t)(2*getBufferSize(gDevice)),
                                      NULL,
                                      &error );
    if( gInBuffer_double == NULL )
    {
        vlog_error( "clCreateArray failed for input (%d)\n", error );
        return -4;
    }
    
    gOutBuffer_half = clCreateBuffer( gContext, 
                                     CL_MEM_WRITE_ONLY, 
                                     (size_t)getBufferSize(gDevice),
                                     NULL,
                                     &error );
    if( gOutBuffer_half == NULL )
    {
        vlog_error( "clCreateArray failed for output (%d)\n", error );
        return -5;
    }
    
    gOutBuffer_single = clCreateBuffer( gContext, 
                                       CL_MEM_WRITE_ONLY, 
                                       (size_t)getBufferSize(gDevice),
                                       NULL,
                                       &error );
    if( gOutBuffer_single == NULL )
    {
        vlog_error( "clCreateArray failed for output (%d)\n", error );
        return -5;
    }
    
    gOutBuffer_double = clCreateBuffer( gContext, 
                                       CL_MEM_WRITE_ONLY, 
                                       (size_t)(2*getBufferSize(gDevice)),
                                       NULL,
                                       &error );
    if( gOutBuffer_double == NULL )
    {
        vlog_error( "clCreateArray failed for output (%d)\n", error );
        return -5;
    }
    
    char string[16384];
    vlog( "\nCompute Device info:\n" );
    error = clGetDeviceInfo(gDevice, CL_DEVICE_NAME, sizeof(string), string, NULL);
    vlog( "\tDevice Name: %s\n", string );
    error = clGetDeviceInfo(gDevice, CL_DEVICE_VENDOR, sizeof(string), string, NULL);
    vlog( "\tVendor: %s\n", string );
    error = clGetDeviceInfo(gDevice, CL_DEVICE_VERSION, sizeof(string), string, NULL);
    vlog( "\tDevice Version: %s\n", string );
    error = clGetDeviceInfo(gDevice, CL_DEVICE_OPENCL_C_VERSION, sizeof(string), string, NULL);
    vlog( "\tOpenCL C Version: %s\n", string );
    error = clGetDeviceInfo(gDevice, CL_DRIVER_VERSION, sizeof(string), string, NULL);
    vlog( "\tDriver Version: %s\n", string );
    vlog( "\tProcessing with %d devices\n", gComputeDevices );
    vlog( "\tDevice Frequency: %d MHz\n", gDeviceFrequency );
    vlog( "\tHas double? %s\n", hasDouble ? "YES" : "NO" );
    vlog( "\tTest double? %s\n", gTestDouble ? "YES" : "NO" );
    
    return 0;
}

cl_program   MakeProgram( const char *source[], int count )
{
    int error;
    int i;
    
    //create the program
    cl_program program = clCreateProgramWithSource( gContext, count, source, NULL, &error );
    if( NULL == program )
    {
        vlog_error( "\t\tFAILED -- Failed to create program. (%d)\n", error );
        return NULL;
    }
    
    // build it
    if( (error = clBuildProgram( program, 1, &gDevice, NULL, NULL, NULL )) )
    {
        size_t  len;
        char    buffer[16384];
        
        vlog_error("\t\tFAILED -- clBuildProgramExecutable() failed:\n");
        clGetProgramBuildInfo(program, gDevice, CL_PROGRAM_BUILD_LOG, sizeof(buffer), buffer, &len);
        vlog_error("Log: %s\n", buffer);
        vlog_error("Source :\n");
        for(i = 0; i < count; ++i) {
            vlog_error("%s", source[i]);
        }
        vlog_error("\n");
        
        clReleaseProgram( program );
        return NULL;
    }
    
    return program;
}

void ReleaseCL(void)
{
    clReleaseMemObject(gInBuffer_half);
    clReleaseMemObject(gOutBuffer_half);
    clReleaseMemObject(gInBuffer_single);
    clReleaseMemObject(gOutBuffer_single);
    clReleaseMemObject(gInBuffer_double);
    clReleaseMemObject(gOutBuffer_double);
    clReleaseCommandQueue(gQueue);
    clReleaseContext(gContext);
}

cl_uint numVecs(cl_uint count, int vectorSizeIdx, bool aligned) {
    if(aligned && g_arrVecSizes[vectorSizeIdx] == 3) {
        return count/4;
    }
    return  (count + g_arrVecSizes[vectorSizeIdx] - 1)/
    ( (g_arrVecSizes[vectorSizeIdx]) );
}

cl_uint runsOverBy(cl_uint count, int vectorSizeIdx, bool aligned) {
    if(aligned || g_arrVecSizes[vectorSizeIdx] != 3) { return -1; }
    return count% (g_arrVecSizes[vectorSizeIdx]);
}

void printSource(const char * src[], int len) {
    int i;
    for(i = 0; i < len; ++i) {
        vlog("%s", src[i]);
    }
}

int RunKernel( cl_kernel kernel, void *inBuf, void *outBuf, uint32_t blockCount , int extraArg)
{
    size_t localCount = blockCount;
    size_t wg_size;
    int error;
    
    error = clSetKernelArg(kernel, 0, sizeof inBuf, &inBuf);
    error |= clSetKernelArg(kernel, 1, sizeof outBuf, &outBuf);
    
    if(extraArg >= 0) {
        error |= clSetKernelArg(kernel, 2, sizeof(cl_uint), &extraArg);
    }
    
    if( error )
    {
        vlog_error( "FAILED -- could not set kernel args\n" );
        return -3;
    }
    
    error = clGetKernelWorkGroupInfo(kernel, gDevice, CL_KERNEL_WORK_GROUP_SIZE, sizeof( wg_size ), &wg_size, NULL);
    if (error)
    {
        vlog_error( "FAILED -- could not get kernel work group info\n" );
        return -4;
    }
    
    wg_size = (wg_size > gWorkGroupSize) ? gWorkGroupSize : wg_size;
    while( localCount % wg_size )
        wg_size--;
    
    if( (error = clEnqueueNDRangeKernel( gQueue, kernel, 1, NULL, &localCount, &wg_size, 0, NULL, NULL )) )
    {
        vlog_error( "FAILED -- could not execute kernel\n" );
        return -5;
    }
    
    return 0;
}

#if defined (__APPLE__ )

#include <mach/mach_time.h>

uint64_t ReadTime( void )
{
    return mach_absolute_time();        // returns time since boot.  Ticks have better than microsecond precsion.
}

double SubtractTime( uint64_t endTime, uint64_t startTime )
{
    static double conversion = 0.0;
    
    if(  0.0 == conversion )
    {
        mach_timebase_info_data_t   info;
        kern_return_t err = mach_timebase_info( &info );
        if( 0 == err )
            conversion = 1e-9 * (double) info.numer / (double) info.denom;
    }
    
    return (double) (endTime - startTime) * conversion;
}

#elif defined( _WIN32 ) && defined (_MSC_VER)

// functions are defined in compat.h

#else

//
//  Please feel free to substitute your own timing facility here. 
//

#warning  Times are meaningless. No timing facility in place for this platform.
uint64_t ReadTime( void )
{
    return 0ULL;
}

// return the difference between two times obtained from ReadTime in seconds
double SubtractTime( uint64_t endTime, uint64_t startTime )
{
    return INFINITY;
}

#endif

#if !defined( __APPLE__ )
void memset_pattern4(void *dest, const void *src_pattern, size_t bytes )
{
    uint32_t pat = ((uint32_t*) src_pattern)[0];
    size_t count = bytes / 4;
    size_t i;
    uint32_t *d = (uint32_t*)dest;
    
    for( i = 0; i < count; i++ )
        d[i] = pat;
    
    d += i;
    
    bytes &= 3;
    if( bytes )
        memcpy( d, src_pattern, bytes );
}
#endif


int is_extension_available( cl_device_id device, const char *extensionName )
{
    char *extString;
    size_t size = 0;
    int err;
    int result = 0;
    
    if(( err = clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, 0, NULL, &size) ))
    {
        vlog_error( "Error: failed to determine size of device extensions string at %s:%d (err = %d)\n", __FILE__, __LINE__, err );
        return 0;
    }
    
    if( 0 == size )
        return 0;
    
    extString = (char*) malloc( size );
    if( NULL == extString )
    {
        vlog_error( "Error: unable to allocate %ld byte buffer for extension string at %s:%d (err = %d)\n", size, __FILE__, __LINE__,  err );
        return 0;
    }
    
    if(( err = clGetDeviceInfo(device, CL_DEVICE_EXTENSIONS, size, extString, NULL) ))
    {
        vlog_error( "Error: failed to obtain device extensions string at %s:%d (err = %d)\n", __FILE__, __LINE__, err );
        free( extString );
        return 0;
    }
    
    if( strstr( extString, extensionName ) )
        result = 1;
    
    free( extString );
    return result;
}

cl_device_fp_config get_default_rounding_mode( cl_device_id device )
{
    char profileStr[128] = "";
    cl_device_fp_config single = 0;
    int error = clGetDeviceInfo( device, CL_DEVICE_SINGLE_FP_CONFIG, sizeof( single ), &single, NULL );
    if( error )
    {
        vlog_error( "Error: Unable to get device CL_DEVICE_SINGLE_FP_CONFIG" );
        return 0;
    }
    
    if( single & CL_FP_ROUND_TO_NEAREST )
        return CL_FP_ROUND_TO_NEAREST;
    
    if( 0 == (single & CL_FP_ROUND_TO_ZERO) )
    {
        vlog_error( "FAILURE: device must support either CL_DEVICE_SINGLE_FP_CONFIG or CL_FP_ROUND_TO_NEAREST" );
        return 0;
    }
    
    // Make sure we are an embedded device before allowing a pass
    if( (error = clGetDeviceInfo( device, CL_DEVICE_PROFILE, sizeof( profileStr ), &profileStr, NULL ) ))
    {
        vlog_error( "FAILURE: Unable to get CL_DEVICE_PROFILE");
        return 0;
    }
    
    if( strcmp( profileStr, "EMBEDDED_PROFILE" ) )
    {
        vlog_error( "FAILURE: non-EMBEDDED_PROFILE devices must support CL_FP_ROUND_TO_NEAREST" );
        return 0;
    }
    
    return CL_FP_ROUND_TO_ZERO;
}

size_t getBufferSize(cl_device_id device_id)
{
    static int s_initialized = 0;
    static cl_device_id s_device_id;
    static cl_ulong s_result = 64*1024;
    
    if(s_initialized == 0 || s_device_id != device_id)
    {
        cl_ulong result;
        cl_int err = clGetDeviceInfo (device_id, 
                                      CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE,
                                      sizeof(result), (void *)&result,
                                      NULL);
        if(err)
        {
            vlog_error("clGetDeviceInfo() failed\n");
            s_result = 64*1024;
            goto exit;
        }
        result = result / 2;
        log_info("Const buffer size is %lx (%ld)\n", result, result);
        s_initialized = 1;
        s_device_id = device_id;
        s_result = result;
    }
    
exit:
    if( s_result > SIZE_MAX )
    {
        vlog_error( "ERROR: clGetDeviceInfo is reporting a CL_DEVICE_MAX_CONSTANT_BUFFER_SIZE larger than addressable memory on the host.\n It seems highly unlikely that this is usable, due to the API design.\n" );
        fflush(stdout);
        abort();
    }
    
    return (size_t) s_result;
}

cl_ulong getBufferCount(cl_device_id device_id, size_t vecSize, size_t typeSize)
{
    cl_ulong tmp = getBufferSize(device_id);
    if(vecSize == 3)
    {
        return tmp/(cl_ulong)(4*typeSize);
    }
    return tmp/(cl_ulong)(vecSize*typeSize);
}
