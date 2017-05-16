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
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#if !defined(_WIN32)
#include <stdint.h>
#include <fenv.h>
#endif

#include <float.h> 
#include <math.h>

#if !defined(_WIN32)
#include <libgen.h>
#include <sys/param.h>
#endif

#include "mingw_compat.h"
#if defined (__MINGW32__)
#include <sys/param.h>
#endif

#include <time.h>
#include "errorHelpers.h"
#include "../../test_common/harness/mt19937.h"
#include "../../test_common/harness/kernelHelpers.h"
#include "../../test_common/harness/rounding_mode.h"
#include "../../test_common/harness/fpcontrol.h"
#if defined( __APPLE__ )
#include <sys/sysctl.h>
#endif
#if defined( __linux__ )
#include <unistd.h>
#include <sys/syscall.h>
#include <linux/sysctl.h>
#endif

#if defined (_WIN32)
#include "../../test_common/harness/compat.h"
#include <string.h>
#endif

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#include <xmmintrin.h>
#endif

#if defined(__PPC__)
// Global varaiable used to hold the FPU control register state. The FPSCR register can not
// be used because not all Power implementations retain or observed the NI (non-IEEE 
// mode) bit.
__thread fpu_control_t fpu_control = 0;
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN  2048
#endif

char                appName[ MAXPATHLEN ] = "";
cl_device_type      gDeviceType = CL_DEVICE_TYPE_DEFAULT;
cl_device_id        gDevice = NULL;
cl_context          gContext = NULL;
cl_command_queue    gQueue = NULL;
cl_program          gProgram[5] = { NULL, NULL, NULL, NULL, NULL };
cl_program          gProgram_double[5] = { NULL, NULL, NULL, NULL, NULL };
int                 gForceFTZ = 0;
int                 gSeed = 0;
int                 gSeedSpecified = 0;
int                 gHasDouble = 0;
MTdata              gMTdata = NULL;
int                 gSkipNanInf = 0;
int		             gIgnoreZeroSign = 0;

cl_mem              bufA = NULL;
cl_mem              bufB = NULL;
cl_mem              bufC = NULL;
cl_mem              bufD = NULL;
cl_mem              bufE = NULL;
cl_mem              bufC_double = NULL;
cl_mem              bufD_double = NULL;
float               *buf1, *buf2, *buf3, *buf4, *buf5, *buf6;
float               *correct[8];
int		             *skipTest[8];

double              *buf3_double, *buf4_double, *buf5_double, *buf6_double;
double              *correct_double[8];

#define BUFFER_SIZE         (1024*1024)


static int ParseArgs( int argc, const char **argv );
static void PrintArch( void );
static void PrintUsage( void );
static int InitCL( void );
static void ReleaseCL( void );
static int RunTest( int testNumber );
static int RunTest_Double( int testNumber );

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
// defeat x87 on MSVC
float sse_add(float x, float y)
{
    volatile float a = x;
    volatile float b = y;
    
    // defeat x87
    __m128 va = _mm_set_ss( (float) a ); 
    __m128 vb = _mm_set_ss( (float) b ); 
    va = _mm_add_ss( va, vb );
    _mm_store_ss( (float*) &a, va );
    return a;
}

double sse_add_sd(double x, double y)
{
    volatile double a = x;
    volatile double b = y;
    
    // defeat x87
    __m128d va = _mm_set_sd( (double) a ); 
    __m128d vb = _mm_set_sd( (double) b ); 
    va = _mm_add_sd( va, vb );
    _mm_store_sd( (double*) &a, va );
    return a;
}

float sse_sub(float x, float y)
{
    volatile float a = x;
    volatile float b = y;
    
    // defeat x87
    __m128 va = _mm_set_ss( (float) a ); 
    __m128 vb = _mm_set_ss( (float) b ); 
    va = _mm_sub_ss( va, vb );
    _mm_store_ss( (float*) &a, va );
    return a;
}

double sse_sub_sd(double x, double y)
{
    volatile double a = x;
    volatile double b = y;
    
    // defeat x87
    __m128d va = _mm_set_sd( (double) a ); 
    __m128d vb = _mm_set_sd( (double) b ); 
    va = _mm_sub_sd( va, vb );
    _mm_store_sd( (double*) &a, va );
    return a;
}

float sse_mul(float x, float y)
{
    volatile float a = x;
    volatile float b = y;
    
    // defeat x87
    __m128 va = _mm_set_ss( (float) a ); 
    __m128 vb = _mm_set_ss( (float) b ); 
    va = _mm_mul_ss( va, vb );
    _mm_store_ss( (float*) &a, va );
    return a;
}

double sse_mul_sd(double x, double y)
{
    volatile double a = x;
    volatile double b = y;
    
    // defeat x87
    __m128d va = _mm_set_sd( (double) a ); 
    __m128d vb = _mm_set_sd( (double) b ); 
    va = _mm_mul_sd( va, vb );
    _mm_store_sd( (double*) &a, va );
    return a;
}
#endif

#ifdef __PPC__
float ppc_mul(float a, float b) 
{ 
    float p; 
    
    if (gForceFTZ) { 
        // Flush input a to zero if it is sub-normal 
        if (fabsf(a) < FLT_MIN) { 
            a = copysignf(0.0, a); 
        } 
        // Flush input b to zero if it is sub-normal 
        if (fabsf(b) < FLT_MIN) { 
            b = copysignf(0.0, b); 
        } 
        // Perform multiply 
        p = a * b; 
        // Flush the product if it is a sub-normal 
        if (fabs((double)a * (double)b) < FLT_MIN) { 
            p = copysignf(0.0, p); 
        } 
    } else { 
        p = a * b; 
    } 
    return p; 
} 
#endif

int main( int argc, const char **argv )
{
    int error  = 0;
    int i;
    
    test_start();
    
    error = ParseArgs( argc, argv );
    if( error )
        return error;
    
    // Init OpenCL
    error = InitCL();
    if( error ) 
        return error;
    
    // run the tests
    log_info( "Testing floats...\n" );
    for( i = 0; i < 8; i++ )
        error |= RunTest( i );
    
    if( gHasDouble )
    {
        log_info( "Testing doubles...\n" );
        for( i = 0; i < 8; i++ )
            error |= RunTest_Double( i );
    }
    
    
    int flush_error = clFinish(gQueue);
    if (flush_error)
        log_error("clFinish failed: %d\n", flush_error);
    
    if( error )
        vlog_error( "Contractions test FAILED.\n" );
    else
        vlog( "Contractions test PASSED.\n" );
    
    ReleaseCL();
    test_finish();
    
    return error;
}



static int ParseArgs( int argc, const char **argv )
{
    int i;
    int length_of_seed = 0;
    
    { // Extract the app name
        strncpy( appName, argv[0], MAXPATHLEN );
        
#if (defined( __APPLE__ ) || defined(__linux__) || defined(__MINGW32__))
        char baseName[MAXPATHLEN];        
        char *base = NULL;
        strncpy( baseName, argv[0], MAXPATHLEN );
        base = basename( baseName );
        if( NULL != base )
        {
            strncpy( appName, base, sizeof( appName )  );
            appName[ sizeof( appName ) -1 ] = '\0';
        }
#elif defined (_WIN32)
        char fname[_MAX_FNAME + _MAX_EXT + 1];
        char ext[_MAX_EXT];
        
        errno_t err = _splitpath_s( argv[0], NULL, 0, NULL, 0, 
                                   fname, _MAX_FNAME, ext, _MAX_EXT );
        if (err == 0) { // no error 
            strcat (fname, ext); //just cat them, size of frame can keep both
            strncpy (appName, fname, sizeof(appName));
            appName[ sizeof( appName ) -1 ] = '\0';
        }
#endif
    }
    
    /* Check if we are forced to CPU mode */
    char *env_mode = getenv( "CL_DEVICE_TYPE" );
    if( env_mode != NULL )
    {
        if( strcmp( env_mode, "gpu" ) == 0 || strcmp( env_mode, "CL_DEVICE_TYPE_GPU" ) == 0 )
            gDeviceType = CL_DEVICE_TYPE_GPU;
        else if( strcmp( env_mode, "cpu" ) == 0 || strcmp( env_mode, "CL_DEVICE_TYPE_CPU" ) == 0 )
            gDeviceType = CL_DEVICE_TYPE_CPU;
        else if( strcmp( env_mode, "accelerator" ) == 0 || strcmp( env_mode, "CL_DEVICE_TYPE_ACCELERATOR" ) == 0 )
            gDeviceType = CL_DEVICE_TYPE_ACCELERATOR;
        else if( strcmp( env_mode, "default" ) == 0 || strcmp( env_mode, "CL_DEVICE_TYPE_DEFAULT" ) == 0 )
            gDeviceType = CL_DEVICE_TYPE_DEFAULT;
        else 
        { 
            vlog_error( "Unknown CL_DEVICE_TYPE env variable setting: %s.\nAborting...\n", env_mode );
            abort();
        }
    }
    
    vlog( "\n%s\t", appName );
    for( i = 1; i < argc; i++ )
    {
        const char *arg = argv[i];
        if( NULL == arg )
            break;
        
        vlog( "\t%s", arg );
        int optionFound = 0;
        if( arg[0] == '-' )
        {
            while( arg[1] != '\0' )
            {
                arg++;
                optionFound = 1;
                switch( *arg )
                {
                    case 'h':
                        PrintUsage();
                        return -1;
                        
                    case 's':
                        arg++;
                        gSeed = atoi( arg ); 
                        while (arg[length_of_seed] >='0' && arg[length_of_seed]<='9')
                            length_of_seed++;
                        gSeedSpecified = 1;
                        arg+=length_of_seed-1;
                        break;
                        
                    case 'z':
                        gForceFTZ ^= 1;
                        break;
                        
                    case ' ':
                        break;
                        
                    default:
                        vlog( " <-- unknown flag: %c (0x%2.2x)\n)", *arg, *arg );
                        PrintUsage();
                        return -1;
                }
            }
        }
        else if( 0 == strcmp( arg, "CL_DEVICE_TYPE_CPU" ) )
            gDeviceType = CL_DEVICE_TYPE_CPU;
        else if( 0 == strcmp( arg, "CL_DEVICE_TYPE_GPU" ) )
            gDeviceType = CL_DEVICE_TYPE_GPU;
        else if( 0 == strcmp( arg, "CL_DEVICE_TYPE_ACCELERATOR" ) )
            gDeviceType = CL_DEVICE_TYPE_ACCELERATOR;
        else if( 0 == strcmp( arg, "CL_DEVICE_TYPE_DEFAULT" ) )
            gDeviceType = CL_DEVICE_TYPE_DEFAULT;
        else
        {
            vlog( "ERROR -- unknown argument: %s\n", arg );
            abort();
        }
    }
    vlog( "\n\nTest binary built %s %s\n", __DATE__, __TIME__ );
    
    PrintArch();
    
    return 0;
}

static void PrintArch( void )
{
    vlog( "\nHost info:\n" );
    vlog( "\tsizeof( void*) = %ld\n", sizeof( void *) );
#if defined( __ppc__ )
    vlog( "\tARCH:\tppc\n" );
#elif defined( __ppc64__ )
    vlog( "\tARCH:\tppc64\n" );
#elif defined( __PPC__ )
    vlog( "ARCH:\tppc\n" );
#elif defined( __i386__ )
    vlog( "\tARCH:\ti386\n" );
#elif defined( __x86_64__ )
    vlog( "\tARCH:\tx86_64\n" );
#elif defined( __arm__ )
    vlog( "\tARCH:\tarm\n" );
#else
    vlog( "\tARCH:\tunknown\n" );
#endif
    
#if defined( __APPLE__ )
    int type = 0;
    size_t typeSize = sizeof( type );
    sysctlbyname( "hw.cputype", &type, &typeSize, NULL, 0 );
    vlog( "\tcpu type:\t%d\n", type );
    typeSize = sizeof( type );
    sysctlbyname( "hw.cpusubtype", &type, &typeSize, NULL, 0 );
    vlog( "\tcpu subtype:\t%d\n", type );
    
#elif defined( __linux__ )
    int _sysctl(struct __sysctl_args *args );
#define OSNAMESZ 100
    
    struct __sysctl_args args;
    char osname[OSNAMESZ];
    size_t osnamelth;
    int name[] = { CTL_KERN, KERN_OSTYPE };
    memset(&args, 0, sizeof(struct __sysctl_args));
    args.name = name;
    args.nlen = sizeof(name)/sizeof(name[0]);
    args.oldval = osname;
    args.oldlenp = &osnamelth;
    
    osnamelth = sizeof(osname);
    
    if (syscall(SYS__sysctl, &args) == -1) {
        vlog( "_sysctl error\n" );
    }
    else {
        vlog("this machine is running %*s\n", osnamelth, osname);
    }
    
#endif
}

static void PrintUsage( void )
{
    vlog( "%s [-z]: <optional: math function names>\n", appName );
    vlog( "\toptions:\n" );
    vlog( "\t\t-z\tToggle FTZ mode (Section 6.5.3) for all functions. (Set by device capabilities by default.)\n" );
    vlog( "\t\t-sNUMBER set random seed.\n");
    vlog( "\n" );
}

const char *sizeNames[] = { "float", "float2", "float4", "float8", "float16" };
const char *sizeNames_double[] = { "double", "double2", "double4", "double8", "double16" };

static void CL_CALLBACK notify_callback(const char *errinfo, const void *private_info, size_t cb, void *user_data)
{
    vlog( "%s\n", errinfo );
}

static int InitCL( void )
{
    cl_platform_id platform = NULL;
    int error;
    uint32_t i, j;
    int *bufSkip = NULL;
    int isRTZ = 0;
    int isEmbedded = 0;
    RoundingMode oldRoundMode = kDefaultRoundingMode;
    
    if( (error = clGetPlatformIDs(1, &platform, NULL) ) )
        return error;
    
    if( (error = clGetDeviceIDs(platform,  gDeviceType, 1, &gDevice, NULL )) )
        return error;
    
    cl_device_fp_config floatCapabilities = 0;
    if( (error = clGetDeviceInfo(gDevice, CL_DEVICE_SINGLE_FP_CONFIG, sizeof(floatCapabilities), &floatCapabilities, NULL)))
        floatCapabilities = 0;
    if(0 == (CL_FP_DENORM & floatCapabilities) )
        gForceFTZ ^= 1;
    
    // check for cl_khr_fp64
    size_t extensions_size = 0;
    if( (error = clGetDeviceInfo( gDevice, CL_DEVICE_EXTENSIONS, 0, NULL, &extensions_size )))
    {
        vlog_error( "clGetDeviceInfo(CL_DEVICE_EXTENSIONS) failed. %d\n", error );
        return -1;
    }
    if( extensions_size )
    {
        char *extensions = (char*)malloc(extensions_size);
        if( NULL == extensions )
        {
            vlog_error( "ERROR: Unable to allocate %ld bytes to hold extensions string\n", extensions_size );
            return -1;
        }
        
        if( (error = clGetDeviceInfo( gDevice, CL_DEVICE_EXTENSIONS, extensions_size, extensions, NULL )))
        {
            vlog_error( "clGetDeviceInfo(CL_DEVICE_EXTENSIONS) failed 2. %d\n", error );
            return -1;
        }
        
        gHasDouble = NULL != strstr( extensions, "cl_khr_fp64" );
        free( extensions );
    }
    
    if(0 == (CL_FP_INF_NAN & floatCapabilities) )
        gSkipNanInf = 1;
    
    char profile[1024] = "";
    if ( (error = clGetDeviceInfo(gDevice, CL_DEVICE_PROFILE, sizeof(profile), profile, NULL ) ) ) {}
    else if (strstr(profile, "EMBEDDED_PROFILE")) 
        isEmbedded = 1;
    
    // Embedded devices that flush to zero are allowed to have an undefined sign.
    if (isEmbedded && gForceFTZ) 
        gIgnoreZeroSign = 1;
    
    gContext = clCreateContext( NULL, 1, &gDevice, notify_callback, NULL, &error );
    if( NULL == gContext || error )
    {
        vlog_error( "clCreateDeviceGroup failed. %d\n", error );
        return -1;
    }
    
    gQueue = clCreateCommandQueue( gContext, gDevice, 0, &error );
    if( NULL == gQueue || error )
    {
        vlog_error( "clCreateContext failed. %d\n", error );
        return -2;
    }
    
    // setup input buffers
    bufA = clCreateBuffer(  gContext,  CL_MEM_READ_WRITE, BUFFER_SIZE, NULL, NULL );
    bufB = clCreateBuffer(  gContext,  CL_MEM_READ_WRITE, BUFFER_SIZE, NULL, NULL );
    bufC = clCreateBuffer(  gContext,  CL_MEM_READ_WRITE, BUFFER_SIZE, NULL, NULL );
    bufD = clCreateBuffer(  gContext,  CL_MEM_READ_WRITE, BUFFER_SIZE, NULL, NULL );
    bufE = clCreateBuffer(  gContext,  CL_MEM_READ_WRITE, BUFFER_SIZE, NULL, NULL );
    
    if( bufA == NULL    || 
       bufB == NULL    || 
       bufC == NULL    || 
       bufD == NULL    || 
       bufE == NULL    )
    {
        vlog_error( "clCreateArray failed for input\n" );
        return -4;
    }
    
    if( gHasDouble )
    {   
        bufC_double = clCreateBuffer(  gContext,  CL_MEM_READ_WRITE, BUFFER_SIZE, NULL, NULL );
        bufD_double = clCreateBuffer(  gContext,  CL_MEM_READ_WRITE, BUFFER_SIZE, NULL, NULL );
        if( bufC_double == NULL    || 
           bufD_double == NULL    )
        {
            vlog_error( "clCreateArray failed for input DP\n" );
            return -4;
        }
    }
    
    const char *kernels[] = {
        "", "#pragma OPENCL FP_CONTRACT OFF\n"
        "__kernel void kernel1( __global ", NULL, " *out, const __global ", NULL, " *a, const __global ", NULL, " *b, const __global ", NULL, " *c )\n"
        "{\n"
        "   int i = get_global_id(0);\n" 
        "   out[i] = a[i] * b[i] + c[i];\n"
        "}\n"
        "\n"
        "__kernel void kernel2( __global ", NULL, " *out, const __global ", NULL, " *a, const __global ", NULL, " *b, const __global ", NULL, " *c )\n"
        "{\n"
        "   int i = get_global_id(0);\n" 
        "   out[i] = a[i] * b[i] - c[i];\n"
        "}\n"
        "\n"
        "__kernel void kernel3( __global ", NULL, " *out, const __global ", NULL, " *a, const __global ", NULL, " *b, const __global ", NULL, " *c )\n"
        "{\n"
        "   int i = get_global_id(0);\n" 
        "   out[i] = c[i] + a[i] * b[i];\n"
        "}\n"
        "\n"
        "__kernel void kernel4( __global ", NULL, " *out, const __global ", NULL, " *a, const __global ", NULL, " *b, const __global ", NULL, " *c )\n"
        "{\n"
        "   int i = get_global_id(0);\n" 
        "   out[i] = c[i] - a[i] * b[i];\n"
        "}\n"
        "\n"
        "__kernel void kernel5( __global ", NULL, " *out, const __global ", NULL, " *a, const __global ", NULL, " *b, const __global ", NULL, " *c )\n"
        "{\n"
        "   int i = get_global_id(0);\n" 
        "   out[i] = -(a[i] * b[i] + c[i]);\n"
        "}\n"
        "\n"
        "__kernel void kernel6( __global ", NULL, " *out, const __global ", NULL, " *a, const __global ", NULL, " *b, const __global ", NULL, " *c )\n"
        "{\n"
        "   int i = get_global_id(0);\n" 
        "   out[i] = -(a[i] * b[i] - c[i]);\n"
        "}\n"
        "\n"
        "__kernel void kernel7( __global ", NULL, " *out, const __global ", NULL, " *a, const __global ", NULL, " *b, const __global ", NULL, " *c )\n"
        "{\n"
        "   int i = get_global_id(0);\n" 
        "   out[i] = -(c[i] + a[i] * b[i]);\n"
        "}\n"
        "\n"
        "__kernel void kernel8( __global ", NULL, " *out, const __global ", NULL, " *a, const __global ", NULL, " *b, const __global ", NULL, " *c )\n"
        "{\n"
        "   int i = get_global_id(0);\n" 
        "   out[i] = -(c[i] - a[i] * b[i]);\n"
        "}\n"
        "\n" };
    
    for( i = 0; i < sizeof( sizeNames ) / sizeof( sizeNames[0] ); i++ )
    {
        size_t strCount = sizeof( kernels ) / sizeof( kernels[0] );
        kernels[0] = "";
        
        for( j = 2; j < strCount; j += 2 )
            kernels[j] = sizeNames[i];
        
        gProgram[i] = clCreateProgramWithSource(gContext, strCount, kernels, NULL, &error);
        if( NULL == gProgram[i] )
        {
            vlog_error( "clCreateProgramWithSource failed\n" );
            return -5;
        }
        
        if(( error = clBuildProgram(gProgram[i], 1, &gDevice, NULL, NULL, NULL) ))
        {
            vlog_error( "clBuildProgramExecutable failed\n" );
            char build_log[2048] = "";
            
            clGetProgramBuildInfo(gProgram[i], gDevice, CL_PROGRAM_BUILD_LOG, sizeof(build_log), build_log, NULL);
            vlog_error( "Log:\n%s\n", build_log );
            return -5;
        }
    }
    
    if( gHasDouble )
    {
        kernels[0] = "#pragma OPENCL EXTENSION cl_khr_fp64 : enable\n";
        for( i = 0; i < sizeof( sizeNames_double ) / sizeof( sizeNames_double[0] ); i++ )
        {
            size_t strCount = sizeof( kernels ) / sizeof( kernels[0] );
            
            for( j = 2; j < strCount; j += 2 )
                kernels[j] = sizeNames_double[i];
            
            gProgram_double[i] = clCreateProgramWithSource(gContext, strCount, kernels, NULL, &error);
            if( NULL == gProgram_double[i] )
            {
                vlog_error( "clCreateProgramWithSource failed\n" );
                return -5;
            }
            
            if(( error = clBuildProgram(gProgram_double[i], 1, &gDevice, NULL, NULL, NULL) ))
            {
                vlog_error( "clBuildProgramExecutable failed\n" );
                char build_log[2048] = "";
                
                clGetProgramBuildInfo(gProgram_double[i], gDevice, CL_PROGRAM_BUILD_LOG, sizeof(build_log), build_log, NULL);
                vlog_error( "Log:\n%s\n", build_log );
                return -5;
            }
        }
    }
    
    if( 0 == gSeedSpecified )
    {
        time_t currentTime = time( NULL );
        struct tm *t = localtime(&currentTime);
        gSeed = t->tm_sec + 60 * ( t->tm_min + 60 * (t->tm_hour + 24 * (t->tm_yday + 365 * t->tm_year))); 
        gSeed = (uint32_t) (((uint64_t) gSeed * (uint64_t) gSeed ) >> 16);
    }
    gMTdata = init_genrand( gSeed );
    
    
    // Init bufA and bufB
    {
        buf1 = (float *)malloc( BUFFER_SIZE );
        buf2 = (float *)malloc( BUFFER_SIZE );
        buf3 = (float *)malloc( BUFFER_SIZE );
        buf4 = (float *)malloc( BUFFER_SIZE );
        buf5 = (float *)malloc( BUFFER_SIZE );
        buf6 = (float *)malloc( BUFFER_SIZE );
        
        bufSkip = (int *)malloc( BUFFER_SIZE );
        
        if( NULL == buf1 || NULL == buf2 || NULL == buf3 || NULL == buf4 || NULL == buf5 || NULL == buf6 || NULL == bufSkip)
        {
            vlog_error( "Out of memory initializing buffers\n" );
            return -15;
        }
        for( i = 0; i < sizeof( correct ) / sizeof( correct[0] ); i++ )
        {
            correct[i] = (float *)malloc( BUFFER_SIZE );
            skipTest[i] = (int *)malloc( BUFFER_SIZE );
            if(( NULL == correct[i] ) || ( NULL == skipTest[i]))
            {
                vlog_error( "Out of memory initializing buffers 2\n" );
                return -15;
            }
        }
        
        for( i = 0; i < BUFFER_SIZE / sizeof(float); i++ )
            ((uint32_t*) buf1)[i] = genrand_int32( gMTdata );
        
        if( (error = clEnqueueWriteBuffer(gQueue, bufA, CL_FALSE, 0, BUFFER_SIZE, buf1, 0, NULL, NULL) ))
        {
            vlog_error( "Failure %d at clEnqueueWriteBuffer1\n", error );
            return error;
        }
        
        for( i = 0; i < BUFFER_SIZE / sizeof(float); i++ )
            ((uint32_t*) buf2)[i] = genrand_int32( gMTdata );
        
        if( (error = clEnqueueWriteBuffer(gQueue, bufB, CL_FALSE, 0, BUFFER_SIZE, buf2, 0, NULL, NULL) ))
        {
            vlog_error( "Failure %d at clEnqueueWriteBuffer2\n", error );
            return error;
        }
        
        void *ftzInfo = NULL;
        if( gForceFTZ )
            ftzInfo = FlushToZero();        
        if ((CL_FP_ROUND_TO_ZERO == get_default_rounding_mode(gDevice)) && isEmbedded) {
            oldRoundMode = set_round(kRoundTowardZero, kfloat);
            isRTZ = 1;
        }
        float *f = (float*) buf1;
        float *f2 = (float*) buf2;
        float *f3 = (float*) buf3;
        float *f4 = (float*) buf4;
        for( i = 0; i < BUFFER_SIZE / sizeof(float); i++ )
        {
            float q = f[i];
            float q2 = f2[i];
            
            feclearexcept(FE_OVERFLOW);
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
            // VS2005 might use x87 for straight multiplies, and we can't
            // turn that off
            f3[i] = sse_mul(q, q2);
            f4[i] = sse_mul(-q, q2);
#elif defined(__PPC__)
            // None of the current generation PPC processors support HW
            // FTZ, emulate it in sw.
            f3[i] = ppc_mul(q, q2);
            f4[i] = ppc_mul(-q, q2);
#else
            f3[i] = q * q2;
            f4[i] = -q * q2;
#endif
            // Skip test if the device doesn't support infinities and NaN AND the result overflows
            // or either input is an infinity of NaN
            bufSkip[i] = (gSkipNanInf && ((FE_OVERFLOW == (FE_OVERFLOW & fetestexcept(FE_OVERFLOW))) ||
                                          (fabsf(q)  == FLT_MAX) || (q  != q)  ||
                                          (fabsf(q2) == FLT_MAX) || (q2 != q2)));
        }
        
        if( gForceFTZ )
            UnFlushToZero(ftzInfo);

	if (isRTZ) 
	  (void)set_round(oldRoundMode, kfloat);        

        
        if( (error = clEnqueueWriteBuffer(gQueue, bufC, CL_FALSE, 0, BUFFER_SIZE, buf3, 0, NULL, NULL) ))
        {
            vlog_error( "Failure %d at clEnqueueWriteBuffer3\n", error );
            return error;
        }
        if( (error = clEnqueueWriteBuffer(gQueue, bufD, CL_FALSE, 0, BUFFER_SIZE, buf4, 0, NULL, NULL) ))
        {
            vlog_error( "Failure %d at clEnqueueWriteBuffer4\n", error );
            return error;
        }
        
        // Fill the buffers with NaN
        float *f5 = (float*) buf5;
        float nan_val = nanf("");
        for( i = 0; i < BUFFER_SIZE / sizeof( float ); i++ )
            f5[i] = nan_val;
        
        // calculate reference results
        for( i = 0; i < BUFFER_SIZE / sizeof( float ); i++ )
        {
            for ( j=0; j<8; j++) 
            {
                feclearexcept(FE_OVERFLOW);
                switch (j) 
                {
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
                        // VS2005 might use x87 for straight add/sub, and we can't
                        // turn that off
                    case 0: 
                        correct[0][i] = sse_add(buf3[i],buf4[i]); break;
                    case 1:
                        correct[1][i] = sse_sub(buf3[i],buf3[i]); break;
                    case 2:
                        correct[2][i] = sse_add(buf4[i],buf3[i]); break;
                    case 3:
                        correct[3][i] = sse_sub(buf3[i],buf3[i]); break;
                    case 4:
                        correct[4][i] = -sse_add(buf3[i],buf4[i]); break;
                    case 5:
                        correct[5][i] = -sse_sub(buf3[i],buf3[i]); break;
                    case 6:
                        correct[6][i] = -sse_add(buf4[i],buf3[i]); break;
                    case 7:
                        correct[7][i] = -sse_sub(buf3[i],buf3[i]); break;
#else
                    case 0:
                        correct[0][i] = buf3[i] + buf4[i]; break;
                    case 1:
                        correct[1][i] = buf3[i] - buf3[i]; break;
                    case 2:
                        correct[2][i] = buf4[i] + buf3[i]; break;
                    case 3:
                        correct[3][i] = buf3[i] - buf3[i]; break;
                    case 4:
                        correct[4][i] = -(buf3[i] + buf4[i]); break;
                    case 5:
                        correct[5][i] = -(buf3[i] - buf3[i]); break;
                    case 6:
                        correct[6][i] = -(buf4[i] + buf3[i]); break;
                    case 7:
                        correct[7][i] = -(buf3[i] - buf3[i]); break;
#endif
                }
                // Further skip test inputs if the device doesn support infinities AND NaNs
                // resulting sum overflows
                skipTest[j][i] = (bufSkip[i] || 
                                  (gSkipNanInf && (FE_OVERFLOW == (FE_OVERFLOW & fetestexcept(FE_OVERFLOW)))));
                
#if defined(__PPC__)
                // Since the current Power processors don't emulate flush to zero in HW,
                // it must be emulated in SW instead.
                if (gForceFTZ) 
                {
                    if ((fabsf(correct[j][i]) < FLT_MIN) && (correct[j][i] != 0.0f)) 
                        correct[j][i] = copysignf(0.0f, correct[j][i]);
                }
#endif
            } 
        }   
        if( gHasDouble )
        {
            // Spec requires correct non-flushed results
            // for doubles. We disable FTZ if this is default on
            // the platform (like ARM) for reference result computation
            // It is no-op if platform default is not FTZ (e.g. x86)
            FPU_mode_type oldMode;
            DisableFTZ( &oldMode );
            
            buf3_double = (double *)malloc( BUFFER_SIZE );
            buf4_double = (double *)malloc( BUFFER_SIZE );
            buf5_double = (double *)malloc( BUFFER_SIZE );
            buf6_double = (double *)malloc( BUFFER_SIZE );
            if( NULL == buf3_double || NULL == buf4_double || NULL == buf5_double || NULL == buf6_double )
            {
                vlog_error( "Out of memory initializing DP buffers\n" );
                return -15;
            }
            for( i = 0; i < sizeof( correct_double ) / sizeof( correct_double[0] ); i++ )
            {
                correct_double[i] = (double *)malloc( BUFFER_SIZE );
                if( NULL == correct_double[i] )
                {
                    vlog_error( "Out of memory initializing DP buffers 2\n" );
                    return -15;
                }
            }
            
            
            double *f  = (double*) buf1;
            double *f2 = (double*) buf2;
            double *f3 = (double*) buf3_double;
            double *f4 = (double*) buf4_double;
            for( i = 0; i < BUFFER_SIZE / sizeof(double); i++ )
            {
                double q = f[i];
                double q2 = f2[i];
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
                // VS2005 might use x87 for straight multiplies, and we can't
                // turn that off
                f3[i] = sse_mul_sd(q, q2);
                f4[i] = sse_mul_sd(-q, q2);
#else
                f3[i] = q * q2;
                f4[i] = -q * q2;
#endif
            }
            
            if( (error = clEnqueueWriteBuffer(gQueue, bufC_double, CL_FALSE, 0, BUFFER_SIZE, buf3_double, 0, NULL, NULL) ))
            {
                vlog_error( "Failure %d at clEnqueueWriteBuffer3\n", error );
                return error;
            }
            if( (error = clEnqueueWriteBuffer(gQueue, bufD_double, CL_FALSE, 0, BUFFER_SIZE, buf4_double, 0, NULL, NULL) ))
            {
                vlog_error( "Failure %d at clEnqueueWriteBuffer4\n", error );
                return error;
            }
            
            // Fill the buffers with NaN
            double *f5 = (double*) buf5_double;
            double nan_val = nanf("");
            for( i = 0; i < BUFFER_SIZE / sizeof( double ); i++ )
                f5[i] = nan_val;
            
            // calculate reference results
            for( i = 0; i < BUFFER_SIZE / sizeof( double ); i++ )
            {
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
                // VS2005 might use x87 for straight add/sub, and we can't
                // turn that off
                correct_double[0][i] = sse_add_sd(buf3_double[i],buf4_double[i]);
                correct_double[1][i] = sse_sub_sd(buf3_double[i],buf3_double[i]);
                correct_double[2][i] = sse_add_sd(buf4_double[i],buf3_double[i]);
                correct_double[3][i] = sse_sub_sd(buf3_double[i],buf3_double[i]);
                correct_double[4][i] = -sse_add_sd(buf3_double[i],buf4_double[i]);
                correct_double[5][i] = -sse_sub_sd(buf3_double[i],buf3_double[i]);
                correct_double[6][i] = -sse_add_sd(buf4_double[i],buf3_double[i]);
                correct_double[7][i] = -sse_sub_sd(buf3_double[i],buf3_double[i]);
#else
                correct_double[0][i] = buf3_double[i] + buf4_double[i];
                correct_double[1][i] = buf3_double[i] - buf3_double[i];
                correct_double[2][i] = buf4_double[i] + buf3_double[i];
                correct_double[3][i] = buf3_double[i] - buf3_double[i];
                correct_double[4][i] = -(buf3_double[i] + buf4_double[i]);
                correct_double[5][i] = -(buf3_double[i] - buf3_double[i]);
                correct_double[6][i] = -(buf4_double[i] + buf3_double[i]);
                correct_double[7][i] = -(buf3_double[i] - buf3_double[i]);
#endif
            }
            
            // Restore previous FP state since we modified it for 
            // reference result computation (see DisableFTZ call above)
            RestoreFPState(&oldMode);
        }
    }
    
    char c[1000];
    static const char *no_yes[] = { "NO", "YES" };
    vlog( "\nCompute Device info:\n" );
    clGetDeviceInfo( gDevice,  CL_DEVICE_NAME, sizeof(c), (void *)&c, NULL);
    vlog( "\tDevice Name: %s\n", c );
    clGetDeviceInfo( gDevice,  CL_DEVICE_VENDOR, sizeof(c), (void *)&c, NULL);
    vlog( "\tVendor: %s\n", c );
    clGetDeviceInfo( gDevice,  CL_DEVICE_VERSION, sizeof(c), (void *)&c, NULL);
    vlog( "\tDevice Version: %s\n", c );
    clGetDeviceInfo( gDevice, CL_DEVICE_OPENCL_C_VERSION, sizeof(c), &c, NULL);
    vlog( "\tCL C Version: %s\n", c );
    clGetDeviceInfo( gDevice,  CL_DRIVER_VERSION, sizeof(c), (void *)&c, NULL);
    vlog( "\tDriver Version: %s\n", c );
    vlog( "\tSubnormal values supported? %s\n", no_yes[0 != (CL_FP_DENORM & floatCapabilities)] );
    vlog( "\tTesting with FTZ mode ON? %s\n", no_yes[0 != gForceFTZ] );
    vlog( "\tTesting Doubles? %s\n", no_yes[0 != gHasDouble] );
    vlog( "\tRandom Number seed: 0x%8.8x\n", gSeed );
    vlog( "\n\n" );
    
    return 0;
}

static void ReleaseCL( void )
{
    clReleaseMemObject(bufA);
    clReleaseMemObject(bufB);
    clReleaseMemObject(bufC);
    clReleaseMemObject(bufD);
    clReleaseMemObject(bufE);
    clReleaseProgram(gProgram[0]);
    clReleaseProgram(gProgram[1]);
    clReleaseProgram(gProgram[2]);
    clReleaseProgram(gProgram[3]);
    clReleaseProgram(gProgram[4]);
    if( gHasDouble )
    {
        clReleaseMemObject(bufC_double);
        clReleaseMemObject(bufD_double);
        clReleaseProgram(gProgram_double[0]);
        clReleaseProgram(gProgram_double[1]);
        clReleaseProgram(gProgram_double[2]);
        clReleaseProgram(gProgram_double[3]);
        clReleaseProgram(gProgram_double[4]);
    }
    clReleaseCommandQueue(gQueue);
    clReleaseContext(gContext);
}


static int RunTest( int testNumber )
{
    size_t i;
    int error = 0;
    cl_mem args[4];
    float *c;
    const char *kernelName[] = { "kernel1", "kernel2", "kernel3", "kernel4", 
        "kernel5", "kernel6", "kernel7", "kernel8" }; 
    switch( testNumber )
    {
        case 0:     args[0] = bufE;     args[1] = bufA;     args[2] = bufB;     args[3] = bufD;     c = buf4;   break;      // a * b + c
        case 1:     args[0] = bufE;     args[1] = bufA;     args[2] = bufB;     args[3] = bufC;     c = buf3;   break;
        case 2:     args[0] = bufE;     args[1] = bufA;     args[2] = bufB;     args[3] = bufD;     c = buf4;   break;
        case 3:     args[0] = bufE;     args[1] = bufA;     args[2] = bufB;     args[3] = bufC;     c = buf3;   break;
        case 4:     args[0] = bufE;     args[1] = bufA;     args[2] = bufB;     args[3] = bufD;     c = buf4;   break;
        case 5:     args[0] = bufE;     args[1] = bufA;     args[2] = bufB;     args[3] = bufC;     c = buf3;   break;
        case 6:     args[0] = bufE;     args[1] = bufA;     args[2] = bufB;     args[3] = bufD;     c = buf4;   break;
        case 7:     args[0] = bufE;     args[1] = bufA;     args[2] = bufB;     args[3] = bufC;     c = buf3;   break;
        default:
            vlog_error( "Unknown test case %d passed to RunTest\n", testNumber );
            return -1;
    }
    
    
    int vectorSize;
    for( vectorSize = 0; vectorSize < 5; vectorSize++ )
    {
        cl_kernel k = clCreateKernel( gProgram[ vectorSize ], kernelName[ testNumber ], &error );
        if( NULL == k || error )
        {
            vlog_error( "%d) Unable to find kernel \"%s\" for vector size: %d\n", error, kernelName[ testNumber ], 1 << vectorSize );
            return -2;
        }
        
        // set the kernel args
        for( i = 0; i < sizeof(args ) / sizeof( args[0]); i++ )
            if( (error = clSetKernelArg(k, i, sizeof( cl_mem ), args + i) ))
            {
                vlog_error( "Error %d setting kernel arg # %ld\n", error, i );
                return error;
            }
        
        // write NaNs to the result array
        if( (error = clEnqueueWriteBuffer(gQueue, bufE, CL_TRUE, 0, BUFFER_SIZE, buf5, 0, NULL, NULL) ))
        {
            vlog_error( "Failure %d at clWriteArray %d\n", error, testNumber );
            return error;
        }
        
        // execute the kernel
        size_t gDim[3] = { BUFFER_SIZE / (sizeof( cl_float ) * (1<<vectorSize)), 0, 0 };
        if( ((error = clEnqueueNDRangeKernel(gQueue, k, 1, NULL, gDim, NULL, 0, NULL, NULL) )))
        {
            vlog_error( "Got Error # %d trying to execture kernel\n", error );
            return error;
        }
        
        // read the data back
        if( (error = clEnqueueReadBuffer(gQueue, bufE, CL_TRUE, 0, BUFFER_SIZE, buf6, 0, NULL, NULL ) ))
        {
            vlog_error( "Failure %d at clReadArray %d\n", error, testNumber );
            return error;
        }
        
        // verify results
        float *test = (float*) buf6;
        float *a = (float*) buf1;
        float *b = (float*) buf2;
        for( i = 0; i < BUFFER_SIZE / sizeof( float ); i++ )
        {
            if( isnan(test[i]) && isnan(correct[testNumber][i] ) )
                continue;
            
            if( skipTest[testNumber][i] )
                continue;
            
            // sign of zero must be correct
            if(( ((uint32_t*) test)[i] != ((uint32_t*) correct[testNumber])[i] ) && 
               !(gIgnoreZeroSign && (test[i] == 0.0f) && (correct[testNumber][i] == 0.0f)) )
            {
                switch( testNumber )
                {
                        // Zeros for these should be positive
                    case 0:     vlog_error( "%ld) Error for %s %s: %a * %a + %a =  *%a vs. %a\n", i, sizeNames[ vectorSize], kernelName[ testNumber ],
                                           a[i], b[i], c[i], correct[testNumber][i], test[i] );       clReleaseKernel(k); return -1;
                    case 1:     vlog_error( "%ld) Error for %s %s: %a * %a - %a =  *%a vs. %a\n", i, sizeNames[ vectorSize], kernelName[ testNumber ],
                                           a[i], b[i], c[i], correct[testNumber][i], test[i] );       clReleaseKernel(k); return -1;
                    case 2:     vlog_error( "%ld) Error for %s %s: %a + %a * %a =  *%a vs. %a\n", i, sizeNames[ vectorSize], kernelName[ testNumber ],
                                           c[i], a[i], b[i], correct[testNumber][i], test[i] );       clReleaseKernel(k); return -1;
                    case 3:     vlog_error( "%ld) Error for %s %s: %a - %a * %a =  *%a vs. %a\n", i, sizeNames[ vectorSize], kernelName[ testNumber ],
                                           c[i], a[i], b[i], correct[testNumber][i], test[i] );       clReleaseKernel(k); return -1;
                        
                        // Zeros for these should be negative
                    case 4:     vlog_error( "%ld) Error for %s %s: -(%a * %a + %a) =  *%a vs. %a\n", i, sizeNames[ vectorSize], kernelName[ testNumber ],
                                           a[i], b[i], c[i], correct[testNumber][i], test[i] );       clReleaseKernel(k); return -1;
                    case 5:     vlog_error( "%ld) Error for %s %s: -(%a * %a - %a) =  *%a vs. %a\n", i, sizeNames[ vectorSize], kernelName[ testNumber ],
                                           a[i], b[i], c[i], correct[testNumber][i], test[i] );       clReleaseKernel(k); return -1;
                    case 6:     vlog_error( "%ld) Error for %s %s: -(%a + %a * %a) =  *%a vs. %a\n", i, sizeNames[ vectorSize], kernelName[ testNumber ],
                                           c[i], a[i], b[i], correct[testNumber][i], test[i] );       clReleaseKernel(k); return -1;
                    case 7:     vlog_error( "%ld) Error for %s %s: -(%a - %a * %a) =  *%a vs. %a\n", i, sizeNames[ vectorSize], kernelName[ testNumber ],
                                           c[i], a[i], b[i], correct[testNumber][i], test[i] );       clReleaseKernel(k); return -1;
                    default:
                        vlog_error( "error: Unknown test number!\n" );
                        clReleaseKernel(k);
                        return -2;
                }
            }
        }
        
        clReleaseKernel(k);
    }
    
    return error;
}

static int RunTest_Double( int testNumber )
{
    size_t i;
    int error = 0;
    cl_mem args[4];
    double *c;
    const char *kernelName[] = { "kernel1", "kernel2", "kernel3", "kernel4", 
        "kernel5", "kernel6", "kernel7", "kernel8" }; 
    
    switch( testNumber )
    {
        case 0:     args[0] = bufE;     args[1] = bufA;     args[2] = bufB;     args[3] = bufD_double;     c = buf4_double;   break;      // a * b + c
        case 1:     args[0] = bufE;     args[1] = bufA;     args[2] = bufB;     args[3] = bufC_double;     c = buf3_double;   break;
        case 2:     args[0] = bufE;     args[1] = bufA;     args[2] = bufB;     args[3] = bufD_double;     c = buf4_double;   break;
        case 3:     args[0] = bufE;     args[1] = bufA;     args[2] = bufB;     args[3] = bufC_double;     c = buf3_double;   break;
        case 4:     args[0] = bufE;     args[1] = bufA;     args[2] = bufB;     args[3] = bufD_double;     c = buf4_double;   break;
        case 5:     args[0] = bufE;     args[1] = bufA;     args[2] = bufB;     args[3] = bufC_double;     c = buf3_double;   break;
        case 6:     args[0] = bufE;     args[1] = bufA;     args[2] = bufB;     args[3] = bufD_double;     c = buf4_double;   break;
        case 7:     args[0] = bufE;     args[1] = bufA;     args[2] = bufB;     args[3] = bufC_double;     c = buf3_double;   break;
        default:
            vlog_error( "Unknown test case %d passed to RunTest\n", testNumber );
            return -1;
    }
    
    int vectorSize;
    for( vectorSize = 0; vectorSize < 5; vectorSize++ )
    {
        cl_kernel k = clCreateKernel( gProgram_double[ vectorSize ], kernelName[ testNumber ], &error );
        if( NULL == k || error )
        {
            vlog_error( "%d) Unable to find kernel \"%s\" for vector size: %d\n", error, kernelName[ testNumber ], 1 << vectorSize );
            return -2;
        }
        
        // set the kernel args
        for( i = 0; i < sizeof(args ) / sizeof( args[0]); i++ )
            if( (error = clSetKernelArg(k, i, sizeof( cl_mem ), args + i) ))
            {
                vlog_error( "Error %d setting kernel arg # %ld\n", error, i );
                return error;
            }
        
        // write NaNs to the result array
        if( (error = clEnqueueWriteBuffer(gQueue, bufE, CL_FALSE, 0, BUFFER_SIZE, buf5_double, 0, NULL, NULL) ))
        {
            vlog_error( "Failure %d at clWriteArray %d\n", error, testNumber );
            return error;
        }
        
        // execute the kernel
        size_t gDim[3] = { BUFFER_SIZE / (sizeof( cl_double ) * (1<<vectorSize)), 0, 0 };
        if( ((error = clEnqueueNDRangeKernel(gQueue, k, 1, NULL, gDim, NULL, 0, NULL, NULL) )))
        {
            vlog_error( "Got Error # %d trying to execture kernel\n", error );
            return error;
        }
        
        // read the data back
        if( (error = clEnqueueReadBuffer(gQueue, bufE, CL_TRUE, 0, BUFFER_SIZE, buf6_double, 0, NULL, NULL ) ))
        {
            vlog_error( "Failure %d at clReadArray %d\n", error, testNumber );
            return error;
        }
        
        // verify results
        double *test = (double*) buf6_double;
        double *a = (double*) buf1;
        double *b = (double*) buf2;
        for( i = 0; i < BUFFER_SIZE / sizeof( double ); i++ )
        {
            if( isnan(test[i]) && isnan(correct_double[testNumber][i] ) )
                continue;
            
            // sign of zero must be correct
            if( ((uint64_t*) test)[i] != ((uint64_t*) correct_double[testNumber])[i] )
            {
                switch( testNumber )
                {
                        // Zeros for these should be positive
                    case 0:     vlog_error( "%ld) Error for %s %s: %a * %a + %a =  *%a vs. %a\n", i, sizeNames_double[ vectorSize], kernelName[ testNumber ],
                                           a[i], b[i], c[i], correct[testNumber][i], test[i] );       return -1;
                    case 1:     vlog_error( "%ld) Error for %s %s: %a * %a - %a =  *%a vs. %a\n", i, sizeNames_double[ vectorSize], kernelName[ testNumber ],
                                           a[i], b[i], c[i], correct[testNumber][i], test[i] );       return -1;
                    case 2:     vlog_error( "%ld) Error for %s %s: %a + %a * %a =  *%a vs. %a\n", i, sizeNames_double[ vectorSize], kernelName[ testNumber ],
                                           c[i], a[i], b[i], correct[testNumber][i], test[i] );       return -1;
                    case 3:     vlog_error( "%ld) Error for %s %s: %a - %a * %a =  *%a vs. %a\n", i, sizeNames_double[ vectorSize], kernelName[ testNumber ],
                                           c[i], a[i], b[i], correct[testNumber][i], test[i] );       return -1;
                        
                        // Zeros for these should be negative
                    case 4:     vlog_error( "%ld) Error for %s %s: -(%a * %a + %a) =  *%a vs. %a\n", i, sizeNames_double[ vectorSize], kernelName[ testNumber ],
                                           a[i], b[i], c[i], correct[testNumber][i], test[i] );       return -1;
                    case 5:     vlog_error( "%ld) Error for %s %s: -(%a * %a - %a) =  *%a vs. %a\n", i, sizeNames_double[ vectorSize], kernelName[ testNumber ],
                                           a[i], b[i], c[i], correct[testNumber][i], test[i] );       return -1;
                    case 6:     vlog_error( "%ld) Error for %s %s: -(%a + %a * %a) =  *%a vs. %a\n", i, sizeNames_double[ vectorSize], kernelName[ testNumber ],
                                           c[i], a[i], b[i], correct[testNumber][i], test[i] );       return -1;
                    case 7:     vlog_error( "%ld) Error for %s %s: -(%a - %a * %a) =  *%a vs. %a\n", i, sizeNames_double[ vectorSize], kernelName[ testNumber ],
                                           c[i], a[i], b[i], correct[testNumber][i], test[i] );       return -1;
                    default:
                        vlog_error( "error: Unknown test number!\n" );
                        return -2;
                }
            }
        }
        
        clReleaseKernel(k);
    }
    
    return error;
}
