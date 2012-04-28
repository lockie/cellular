
#include <stdio.h>
#include <curand_kernel.h>

#include "automaton.h"


// The dimension block_size of is chosen equal to 16, so that the number of
//  threads per block is a multiple of the warp size and remains below the
//  maximum number of threads per block.
#define BLOCK_SIZE 16

#define CUDA_SAFE_CALL(call) {                                               \
	cudaError err = call;                                                    \
	if( cudaSuccess != err) {                                                \
		fprintf(stderr, "Cuda error in file '%s' in line %i : %s.\n",        \
				__FILE__, __LINE__, cudaGetErrorString( err) );              \
		exit(EXIT_FAILURE);                                                  \
	} }

struct Rule_dev
{
#define MAX_RULE_LENGTH 8
	char oldstate[MAX_RULE_LENGTH];
	char newstate[MAX_RULE_LENGTH];
	float probability;
};

__global__ void srandKernel(curandState* state, const size_t width,
	const int seed)
{
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;
	int idx = y * width + x;
	/* Each thread gets same seed, a different sequence
	number, no offset */
	curand_init(seed, idx, 0, &state[idx]);
}

// This kernels runs for each row, 'height' times
__global__ void partitionHorizontalKernel(curandState* state,
	const size_t width, const size_t height, float omega, int* partition)
{
	int y = blockIdx.y * blockDim.y + threadIdx.y;
	if(y >= height)
		return;

	for(int x = 0; x < width;)
	{
		int idx = y * width + x;
		int len = 1;
		while(float(curand(&state[idx])) / UINT_MAX < omega &&
			(len + x < width))
			len++;
		partition[idx] = len;
		for(int i = 1; i < len; i++)
			partition[idx+i] = 0;
		x += len;
	}
}

// This kernels runs for each column, 'width' times
__global__ void partitionVerticalKernel(curandState* state,
	const size_t width, const size_t height, float omega, int* partition)
{
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	if(x >= width)
		return;

	for(int y = 0; y < height;)
	{
		int idx = y * width + x;
		int len = 1;
		while(float(curand(&state[idx])) / UINT_MAX < omega &&
			(len + y < height))
			len++;
		partition[idx] = len;
		for(int i = 1; i < len; i++)
			partition[(y+i)*width+x] = 0;
		y += len;
	}
}

__global__ void tickHorizontalKernel(curandState* state, char* lattice,
	const size_t width, const size_t height,
	const Rule_dev* rules, const size_t nRules, const int* partition)
{
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;
	if(x >= width || y >= height)
		return;
	int idx = y * width + x;
	int len = partition[idx];
	if(len == 0)
		return;

	float rnd = float(curand(&state[idx])) / UINT_MAX;
	for(int i = 0; i < nRules; i++)
	{
		bool will_aply = true;
		int l;
		for(l = 0; l < MAX_RULE_LENGTH; l++)
		{
			if(rules[i].oldstate[l] == 0) // CUDA cannot into strlen(3)
				break;
			if(rules[i].oldstate[l] != lattice[idx+l])
			{
				will_aply = false;
				break;
			}
		}
		if(l != len)
			continue;
		if(!will_aply)
			continue;
		if(rnd < rules[i].probability)
		{
			for(int k = 0; k < l; k++)
				lattice[idx+k] = rules[i].newstate[k];
			break;
		}
	}
}

__global__ void tickVerticalKernel(curandState* state, char* lattice,
	const size_t width, const size_t height,
	const Rule_dev* rules, const size_t nRules, const int* partition)
{
	int x = blockIdx.x * blockDim.x + threadIdx.x;
	int y = blockIdx.y * blockDim.y + threadIdx.y;
	if(x >= width || y >= height)
		return;
	int idx = y * width + x;
	int len = partition[idx];
	if(len == 0)
		return;

	float rnd = float(curand(&state[idx])) / UINT_MAX;
	for(int i = 0; i < nRules; i++)
	{
		bool will_aply = true;
		int l;
		for(l = 0; l < MAX_RULE_LENGTH; l++)
		{
			if(rules[i].oldstate[l] == 0) // CUDA cannot into strlen(3)
				break;
			if(rules[i].oldstate[l] != lattice[(y+l)*width + x])
			{
				will_aply = false;
				break;
			}
		}
		if(l != len)
			continue;
		if(!will_aply)
			continue;
		if(rnd < rules[i].probability)
		{
			for(int k = 0; k < l; k++)
				lattice[(y+k)*width+x] = rules[i].newstate[k];
			break;
		}
	}
}

static curandState* srand_dev = NULL;
static Rule_dev* rules = NULL;
static Rule_dev* rules_dev = NULL;
static size_t nrules = 0;
static char* lattice_dev = NULL;
static int* partition_dev = NULL;
static size_t size = 0;

extern "C"
__host__ void tick_cuda(struct Automaton* automaton, size_t steps)
{
	size_t k;
	Rule* r;

	int i;
	dim3 dimBlock(BLOCK_SIZE, BLOCK_SIZE),
		dimGrid(automaton->width / dimBlock.x, automaton->height / dimBlock.y),
		dimHorizontalBlock(1, BLOCK_SIZE),
		dimHorizontalGrid(1, automaton->height / dimHorizontalBlock.y),
		dimVerticalBlock(BLOCK_SIZE, 1),
		dimVerticalGrid(automaton->width / dimVerticalBlock.x, 1);
	if(automaton->width % BLOCK_SIZE != 0)
	{
		dimGrid.x++;
		dimVerticalGrid.x++;
	}
	if(automaton->height % BLOCK_SIZE != 0)
	{
		dimGrid.y++;
		dimHorizontalGrid.y++;
	}

	if(!automaton->rules)
		return;

	automaton->ticks++;

	/* Выделить память, скопировать правила и проинициализировать ГСЧ,
	 если нужно */
	if(size < automaton->size)
	{
		if(srand_dev)
			cudaFree(srand_dev);
		if(partition_dev)
			cudaFree(partition_dev);
		if(lattice_dev)
			cudaFree(lattice_dev);

		size = automaton->size;
		CUDA_SAFE_CALL(cudaMalloc((void**)&srand_dev,
			size * sizeof(curandState)));
		CUDA_SAFE_CALL(cudaMalloc((void**)&partition_dev,
			size * sizeof(int)));
		CUDA_SAFE_CALL(cudaMalloc((void**)&lattice_dev,
			size));

		srandKernel<<<dimGrid, dimBlock>>>(srand_dev, automaton->width,
			(int)time(NULL));

		if(nrules != 0)
		{
			cudaFree(rules_dev);
			cudaFree(rules);
		}
		nrules = 0;
		for(r = automaton->rules; r != NULL; r = r->next)
			nrules++;
		CUDA_SAFE_CALL(cudaMallocHost((void**)&rules,
			nrules * sizeof(Rule_dev)));
		for(i = 0, r = automaton->rules; r != NULL; r = r->next, i++)
		{
			strncpy(rules[i].oldstate, r->oldstate, MAX_RULE_LENGTH);
			strncpy(rules[i].newstate, r->newstate, MAX_RULE_LENGTH);
			rules[i].probability = (float)r->probability;
		}
		CUDA_SAFE_CALL(cudaMalloc((void**)&rules_dev,
			nrules * sizeof(Rule_dev)));
		CUDA_SAFE_CALL(cudaMemcpy(rules_dev, rules, nrules * sizeof(Rule_dev),
			cudaMemcpyHostToDevice));
	}

	cudaMemcpy(lattice_dev, automaton->lattice, automaton->size,
		cudaMemcpyHostToDevice);

	for(k = 0; k < steps; k++)
	{
		if(rand() > RAND_MAX/2)
		{
			partitionHorizontalKernel<<<dimHorizontalGrid, dimHorizontalBlock>>>(
				srand_dev,
				automaton->width, automaton->height,
				automaton->omega, partition_dev);
			tickHorizontalKernel<<<dimGrid, dimBlock>>>(srand_dev, lattice_dev,
				automaton->width, automaton->height,
				rules_dev, nrules, partition_dev);
		}
		else
		{
			partitionVerticalKernel<<<dimVerticalGrid, dimVerticalBlock>>>(
				srand_dev,
				automaton->width, automaton->height,
				automaton->omega, partition_dev);
			tickVerticalKernel<<<dimGrid, dimBlock>>>(srand_dev, lattice_dev,
				automaton->width, automaton->height,
				rules_dev, nrules, partition_dev);
		}
	}

	cudaMemcpy(automaton->lattice, lattice_dev, automaton->size,
		cudaMemcpyDeviceToHost);
}

