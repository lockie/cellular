
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <getopt.h>

#include "automaton.h"
#include "video.h"


static const char* opt_string = "i:o:s:w:vh?";
static const struct option long_opts[] = {
	{ "infile", 	required_argument,	NULL,	'i' },
	{ "outfile",	required_argument,	NULL,	'o' },
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
	void* renderer;
	int i, verbose = 0, idx = 0;
	const char *infile = NULL, *outfile = NULL, *watermark = NULL;
	int steps = 0;

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

	/* Загрузить автомат */
	automaton = load_automaton(infile);
	if(!automaton)
		return EXIT_FAILURE;

	/* Открыть выходной файл */
	renderer = open_renderer(automaton, outfile);
	if(!renderer)
		return EXIT_FAILURE;

	/* Провести симуляцию */
	srand(time(NULL));
	for(i = 0; i < steps; i++)
	{
		render(renderer, automaton, watermark);
		tick(automaton);
		if(verbose)
			printf("Tick %4d/%d\r", automaton->ticks, steps);
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
	printf("\t-o, --outfile=<file>\tOutput file (video in mkv container)\n");
	printf("\t-s, --steps=<number>\tNumber of simulation steps (defaults to 100)\n");
	printf("\t-w, --watermark=<text>\tWatermark printed as a subtitle on each video frame\n");
	printf("\t-v, --verbose\t\tBe verbose (show fancy progressbar)\n");
	printf("\t-h, -?, --help\t\tShow handy user manual you're reading\n");

	printf("\nCellular Copyright (C) 2011-2012 Andrew Kravchuk\n\
This is free software; see the source for copying conditions.  There is NO\n\
warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n");
}

