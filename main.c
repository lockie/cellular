
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "automaton.h"
#include "video.h"


/** Точка входа
**/
int main(int argc, char** argv)
{
	struct Automaton* automaton;
	void* renderer;
	int i;

	if(argc != 4 && argc != 5)
	{
		printf(
"USAGE: %s <input automaton xmlfile> <output videofile> <tick count> [watermark]\n",
			argv[0]);
		return EXIT_SUCCESS;
	}

	/* Загрузить автомат */
	automaton = load_automaton(argv[1]);
	if(!automaton)
		return EXIT_FAILURE;

	/* Открыть выходной файл */
	renderer = open_renderer(automaton, argv[2]);
	if(!renderer)
		return EXIT_FAILURE;

	/* Провести симуляцию */
	srand(time(NULL));
	for(i = 0; i < atoi(argv[3]); i++)
	{
		render(renderer, automaton, argc == 5 ? argv[4] : NULL);
		tick(automaton);
	}

	/* Прибраться */
	close_renderer(renderer);
	delete_automaton(&automaton);
	return EXIT_SUCCESS;
}

