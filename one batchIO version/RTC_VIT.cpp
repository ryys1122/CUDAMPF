/*
 *
 */

#include "header_def.h"
#include "nvrtc_options.h"		// include <string>

void RTC_VIT(unsigned int number, const char* GPU_kernel, HMMER_PROFILE *hmm,
	     unsigned int *seq_1D, unsigned int *offset, unsigned int *seq_len,
	     unsigned int *iLen, unsigned int sum, double *pVal,
	     int warp, int maxreg, dim3 GRID, dim3 BLOCK)
{	
	/*********************************/
	/* 0. Prepare for cuda drive API */
	/*********************************/
	CUdevice cuDevice;
	CUcontext context;
	CUmodule module;
	CUfunction kernel;

	checkCudaErrors(cuInit(0));
	checkCudaErrors(cuDeviceGet(&cuDevice, 0));
	checkCudaErrors(cuCtxCreate(&context, 0, cuDevice)); 

	/*********************************************/
	/* 1. Device Property: fixed based on Device */
	/*********************************************/

	/****************************************/
	/* 2. Device Memory Allocation and copy */
	/****************************************/
	StopWatchInterface *timer;
    sdkCreateTimer(&timer);
    sdkStartTimer(&timer);

   	/* Driver API pointers */
	CUdeviceptr d_seq, d_offset, d_len, d_len_6r, mat_v, trans, score;

	/* Allocation */
	checkCudaErrors(cuMemAlloc(&d_seq, sum * sizeof(unsigned int)));							/* copy 1D database */
	checkCudaErrors(cuMemAlloc(&d_offset, number * sizeof(unsigned int)));						/* copy offset of each seq*/
	checkCudaErrors(cuMemAlloc(&d_len, number * sizeof(unsigned int)));							/* copy raw length of each seq */
	checkCudaErrors(cuMemAlloc(&d_len_6r, number * sizeof(unsigned int)));						/* copy padding length of each seq */
	checkCudaErrors(cuMemAlloc(&mat_v, hmm->vitQ * PROTEIN_TYPE * sizeof(__32int__)));		/* striped EMISSION score */
	checkCudaErrors(cuMemAlloc(&trans, hmm->vitQ * TRANS_TYPE * sizeof(__32int__)));		/* striped transition score */
	checkCudaErrors(cuMemAlloc(&score, number * sizeof(double)));								/* P-Value as output */

	/* H to D copy */
	checkCudaErrors(cuMemcpyHtoD(d_seq, seq_1D, sum * sizeof(unsigned int)));
	checkCudaErrors(cuMemcpyHtoD(d_offset, offset, number * sizeof(unsigned int)));
	checkCudaErrors(cuMemcpyHtoD(d_len, seq_len, number * sizeof(unsigned int)));
	checkCudaErrors(cuMemcpyHtoD(d_len_6r, iLen, number * sizeof(unsigned int)));
	checkCudaErrors(cuMemcpyHtoD(mat_v, hmm->vit_vec, hmm->vitQ * PROTEIN_TYPE * sizeof(__32int__)));
	checkCudaErrors(cuMemcpyHtoD(trans, hmm->trans_vec, hmm->vitQ * TRANS_TYPE * sizeof(__32int__)));
		
	sdkStopTimer(&timer);
    printf("Alloc & H to D Copy time: %f (ms)\n", sdkGetTimerValue(&timer));
    sdkDeleteTimer(&timer);

    /********************************************************/
	/* 3. Runtime compilation, Generate PTX and Load module */
	/********************************************************/
    sdkCreateTimer(&timer);
    sdkStartTimer(&timer);

	/* NVRTC create handle */
	nvrtcProgram prog;
	NVRTC_SAFE_CALL("nvrtcCreateProgram", nvrtcCreateProgram(&prog,				// prog
															 GPU_kernel,		// buffer
															 NULL,				// name: CUDA program name. name can be NULL; “default_program” is used when it is NULL.
															 0,					// numHeaders (I put header file path with -I later)
															 NULL,				// headers' content
															 NULL));			// include full name of headers

	/* 1. eliminate const through pointer */
    char *a = NULL;
    const char *b = a;
    const char **opts = &b;

    /* 2. elminate const through reference */
    //char a_value = 'c';
    //char* aa = &a_value;
    //const char *&bb = aa;		// no way with const
    //const char**&ref = aa;	// no way

    /* Dynamic Options */
    char **test_char = new char*[8];

    test_char[0] = new char[__INCLUDE__.length() + strlen("simd_def.h") + 1];					// #include simd_def.h
	strcpy(test_char[0], get_option(__INCLUDE__, "simd_def.h").c_str());

    test_char[1] = new char[__INCLUDE__.length() + strlen("simd_functions.h") + 1];				// #include simd_functions.h
    strcpy(test_char[1], get_option(__INCLUDE__, "simd_functions.h").c_str());

    test_char[2] = new char[__RDC__.length() + __F__.length() + 1];								// -rdc=false
    strcpy(test_char[2], get_option(__RDC__, __F__).c_str());

    test_char[3] = new char[__ARCH__.length() + __CC35__.length() + 1];							// -arch=compute_35
    strcpy(test_char[3], get_option(__ARCH__, __CC35__).c_str());

    test_char[4] = new char[__MAXREG__.length() + int2str(maxreg).length() + 1];				// -maxrregcount = <?>
    strcpy(test_char[4], get_option(__MAXREG__, int2str(maxreg)).c_str());

    test_char[5] = new char[__RIB__.length() + int2str(warp).length() + 1];						// #define RIB <?> : warps per block
    strcpy(test_char[5], get_option(__RIB__, int2str(warp)).c_str());

    test_char[6] = new char[__SIZE__.length() + int2str((int)force_local_size).length() + 1];	// #define SIZE 40
    strcpy(test_char[6], get_option(__SIZE__, int2str((int)force_local_size)).c_str());

    test_char[7] = new char[__Q__.length() + int2str(hmm->vitQ).length() + 1];					// #define Q <?>
    strcpy(test_char[7], get_option(__Q__, int2str(hmm->vitQ)).c_str());

    /* 1. change const char** through pointer */
    //char* **test = const_cast<char** *>(&opts);
    //*test = test_char;

    /* 2. change const char** through reference */
    char** &test_ref = const_cast<char** &>(opts);
    test_ref = test_char;

    /* NVRTC compile */
	NVRTC_SAFE_CALL("nvrtcCompileProgram", nvrtcCompileProgram(prog,	// prog
															   8,		// numOptions
															   opts));	// options

	sdkStopTimer(&timer);
    printf("nvrtc Creat and Compile: %f (ms)\n", sdkGetTimerValue(&timer));
    sdkDeleteTimer(&timer);

	//======================================================================================//
	// /* dump log */																		//	
    // size_t logSize;																		//
    // NVRTC_SAFE_CALL("nvrtcGetProgramLogSize", nvrtcGetProgramLogSize(prog, &logSize));	//
    // char *log = (char *) malloc(sizeof(char) * logSize + 1);								//
    // NVRTC_SAFE_CALL("nvrtcGetProgramLog", nvrtcGetProgramLog(prog, log));				//
    // log[logSize] = '\x0';																//
    // std::cerr << "\n compilation log ---\n";												//
    // std::cerr << log;																	//
    // std::cerr << "\n end log ---\n";														//
    // free(log);																			//
	//======================================================================================//
	
	/* NVRTC fetch PTX */
	sdkCreateTimer(&timer);
    sdkStartTimer(&timer);

	size_t ptxsize;
	NVRTC_SAFE_CALL("nvrtcGetPTXSize", nvrtcGetPTXSize(prog, &ptxsize));
	char *ptx = new char[ptxsize];
	NVRTC_SAFE_CALL("nvrtcGetPTX", nvrtcGetPTX(prog, ptx));
	NVRTC_SAFE_CALL("nvrtcDestroyProgram", nvrtcDestroyProgram(&prog));	// destroy program instance

	/* Launch PTX by driver API */
	checkCudaErrors(cuModuleLoadDataEx(&module, ptx, 0, 0, 0));
	checkCudaErrors(cuModuleGetFunction(&kernel, module, "KERNEL"));	// return the handle of function, name is the same as real kernel function

	sdkStopTimer(&timer);
    printf("Compile & Load time: %f (ms)\n", sdkGetTimerValue(&timer));
    sdkDeleteTimer(&timer);

    /**************************************/
	/* 4. GPU kernel launch by driver API */
	/**************************************/
    sdkCreateTimer(&timer);
    sdkStartTimer(&timer);
    
    cuCtxSetCacheConfig(CU_FUNC_CACHE_PREFER_L1);
   /* parameters for kernel funciton */
	void *arr[] = { &d_seq, &number, &d_offset,
					&score, &d_len, &d_len_6r, &mat_v, &trans, 
					&(hmm->base_vs), &(hmm->E_lm), &(hmm->ddbound_vs),
					&(hmm->scale_w), &(hmm->vitQ), &(hmm->MU[1]), &(hmm->LAMBDA[1])};

	/* launch kernel */
        checkCudaErrors(cuLaunchKernel(	kernel,
								  	GRID.x, GRID.y, GRID.z,		/* grid dim */
									BLOCK.x, BLOCK.y, BLOCK.z,	/* block dim */
									0,0,						/* SMEM, stream */
									&arr[0],					/* kernel params */
									0));						/* extra opts */

	/* wait for kernel finish */
	checkCudaErrors(cuCtxSynchronize());			/* block for a context's task to complete */

	sdkStopTimer(&timer);
    printf("Kernel time: %f (ms)\n", sdkGetTimerValue(&timer));
    sdkDeleteTimer(&timer);

    /*****************************************/
    /* 5. P-value return and post-processing */
    /*****************************************/
    sdkCreateTimer(&timer);
    sdkStartTimer(&timer);

    checkCudaErrors(cuMemcpyDtoH(pVal, score, number * sizeof(double)));

   	sdkStopTimer(&timer);
    printf("D to H copy time: %f (ms)\n", sdkGetTimerValue(&timer));
    sdkDeleteTimer(&timer);

    /* count the number of seqs pass */
	unsigned long pass_vit = 0;			/* # of seqs pass vit */

	for (int i = 0; i < number; i++)
	{
		if (pVal[i] <= F2)
			pass_vit++;
	}

	printf("|			PASS VIT 			\n");
	printf("|	 ALL	|	 FWD	|\n");
	printf("|	%d  	|	%d  	|\n",  pass_vit, pass_vit);

	/************************/
	/* 6. clean the context */
	/************************/
    checkCudaErrors(cuDevicePrimaryCtxReset(cuDevice));		/* reset */
	checkCudaErrors(cuCtxSynchronize());					/* block for a context's task to complete */
}
