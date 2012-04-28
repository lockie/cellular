
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <getopt.h>

#include "automaton.h"
#include "video.h"


static const char* opt_string = "i:o:cCts:w:vh?";
static const struct option long_opts[] = {
	{ "infile", 	required_argument,	NULL,	'i' },
	{ "outfile",	required_argument,	NULL,	'o' },
	{ "characters",	no_argument,		NULL,	'c' },
	{ "cpu",		no_argument,		NULL,	'C' },
	{ "timings",	no_argument,		NULL,	't' },
	{ "steps", 		required_argument,	NULL,	's' },
	{ "watermark",	required_argument,	NULL,	'w' },
	{ "verbose",	no_argument,		NULL,	'v' },
	{ "help",		no_argument,		NULL,	'h' },
	{ NULL,			no_argument,		NULL,	0   }
};

void display_usage(const char*);

/** Точка входа
**/
int main(int argc, char** argv)
{
	struct Automaton* automaton;
	void* renderer = NULL;
	int i, verbose = 0, idx = 0, characters = 0, CPU = 0, timings = 0;
	const char *infile = NULL, *outfile = NULL, *watermark = NULL;
	int steps = 0;
	struct timespec start, end;

	/* Разобрать параметры */
	int r = getopt_long(argc, argv, opt_string, long_opts, &idx);
	while(r != -1)
	{
		switch(r)
		{
			case 'i':
				infile = optarg;
				break;
			case 'o':
				outfile = optarg;
				break;
			case 'c':
				characters = 1;
				break;
			case 'C':
				CPU = 1;
				break;
			case 't':
				timings = 1;
				break;
			case 's':
				steps = atoi(optarg);
				break;
			case 'w':
				watermark = optarg;
				break;
			case 'v':
				verbose++;
				break;
			case 'h':
			case '?':
				display_usage(argv[0]);
				return EXIT_SUCCESS;
				break;
			default:
				break;
		}
		r = getopt_long(argc, argv, opt_string, long_opts, &idx);
	}
	if(!infile)
	{
		printf("No input files\n");
		return EXIT_SUCCESS;
	}
	if(!outfile)
	{
		printf("No output file\n");
		return EXIT_SUCCESS;
	}
	if(steps == 0)
		steps = 100;
	if(watermark && characters)
	{
		printf("--watermark option is incompatible with --characters\n");
		return EXIT_SUCCESS;
	}

	/* Загрузить автомат */
	automaton = load_automaton(infile);
	if(!automaton)
		return EXIT_FAILURE;

	/* Открыть выходной файл */
	if(!characters)
	{
		renderer = open_renderer(automaton, outfile);
		if(!renderer)
			return EXIT_FAILURE;
	}

	/* Провести симуляцию */
	srand(time(NULL));
	clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	if(characters && !CPU)
		tick_cuda(automaton, steps);
	else
	{
		for(i = 0; i < steps; i++)
		{
			if(!characters)
				render(renderer, automaton, watermark);
			if(CPU)
				tick(automaton);
			else
				tick_cuda(automaton, 1);
			if(verbose)
				printf("Tick %4d/%d\r", automaton->ticks, steps);
		}
	}
	clock_gettime(CLOCK_MONOTONIC, &end);

	if(characters)
		save_automaton(automaton, outfile);

	if(timings)
	{
		double elapsed = (end.tv_sec - start.tv_sec) +
			(double)(end.tv_nsec - start.tv_nsec) / 1.0e9;
		printf("Took %f s\n", elapsed);
	}

	/* Прибраться */
	close_renderer(renderer);
	delete_automaton(&automaton);
	return EXIT_SUCCESS;
}

void display_usage(const char* progname)
{
	printf("USAGE: %s [options]\n", progname);
	printf("Options:\n");
	printf("\t-i, --infile=<file>\tInput file (cellular automaton in XML format)\n");
	printf("\t-o, --outfile=<file>\tOutput file (video in mkv container/automaton in XML)\n");
	printf("\t-c, --characters\tWrite final state of cellular automaton in XML format instead of video\n");
	printf("\t-C, --cpu\t\tForce CPU (non-CUDA) implementation\n");
	printf("\t-t, --timings\t\tDo timings\n");
	printf("\t-s, --steps=<number>\tNumber of simulation steps (defaults to 100)\n");
	printf("\t-w, --watermark=<text>\tWatermark printed as a subtitle on each video frame\n");
	printf("\t-v, --verbose\t\tBe verbose (show fancy progressbar)\n");
	printf("\t-h, -?, --help\t\tShow handy user manual you're reading\n");

	printf("\nCellular Copyright (C) 2011-2012 Andrew Kravchuk\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");
}

