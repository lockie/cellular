
#include <string.h>
#include <float.h>
#include <math.h>

#include <libxml/parser.h>

#include "automaton.h"


struct Automaton* load_automaton(const char* filename)
{
	xmlDoc* doc;
	xmlNode *root, *node;
	xmlChar* str;
	int x, y;
	double p;
	struct Rule *rule = NULL, *last_rule = NULL, *tmprule = NULL, *similar_rule;
	struct Automaton* automaton =
		(struct Automaton*)malloc(sizeof(struct Automaton));
	if(!automaton)
		return NULL;
	memset(automaton, 0, sizeof(struct Automaton));

	/* Проинициализировать libxml и проверить на возможные несовпадения ABI */
	LIBXML_TEST_VERSION

	/* Загрузить файл */
	doc = xmlReadFile(
		filename,
		NULL,
		XML_PARSE_NONET | XML_PARSE_PEDANTIC);
	if(!doc)
		return NULL;

	/* TODO : проверка на соответствие xml-схеме */

	/* Получить свойства автомата */
	root = xmlDocGetRootElement(doc);

	/* Параметр \omega */
	str = xmlGetProp(root, (xmlChar*)"omega");
	if(!str)
		automaton->omega = 0.5;
	else
		automaton->omega = atof((char*)str);

	/* Ширина поля, по умолчанию 100 */
	str = xmlGetProp(root, (xmlChar*)"width");
	if(!str)
		automaton->width = 100;
	else
		automaton->width = atoi((char*)str);
	xmlFree(str);

	/* Высота поля, по умолчанию 100 */
	str = xmlGetProp(root, (xmlChar*)"height");
	if(!str)
		automaton->height = 100;
	else
		automaton->height = atoi((char*)str);
	xmlFree(str);

	/* Создать поле */
	automaton->size = automaton->height * automaton->width;
	automaton->lattice = (char*)malloc(automaton->size);

	/* Загрузить состояния, правила и клетки автомата */
	for(node = root->children; node != root->last; node = node->next)
	{
		if(xmlStrcmp(node->name, (xmlChar*)"zerostate") == 0) /* Нулевое состояние */
		{
			str = xmlGetProp(node, (xmlChar*)"name");
			if(!str)
				str = (xmlChar*)"0";
			automaton->zerostate = str[0];
			xmlFree(str);
			/* Заполнить поле нулевым состоянием */
			memset(automaton->lattice, automaton->zerostate, automaton->size);
		}
		else if(xmlStrcmp(node->name, (xmlChar*)"state") == 0) /* Обычное состояние */
		{
			str = xmlGetProp(node, (xmlChar*)"name");
			if(!str)
			{
				automaton->states[automaton->nstates] =
					automaton->nstates + 47; /* ASCII HACK */
				automaton->nstates++;
			}
			else
				automaton->states[automaton->nstates++] =
					str[0];
			xmlFree(str);
		}
		else if(xmlStrcmp(node->name, (xmlChar*)"rule") == 0) /* Правило */
		{
			rule = (struct Rule*)malloc(sizeof(struct Rule));

			/* Состояние на текущем шаге */
			str = xmlGetProp(node, (xmlChar*)"oldstate");
			if(!str)
				continue;
			rule->oldstate = (char*)malloc(xmlStrlen(str));
			strcpy(rule->oldstate, (char*)str);
			xmlFree(str);

			/* Состояние на следующем шаге */
			str = xmlGetProp(node, (xmlChar*)"newstate");
			if(!str)
				continue;
			rule->newstate = (char*)malloc(xmlStrlen(str));
			strcpy(rule->newstate, (char*)str);
			xmlFree(str);

			if(strlen(rule->oldstate) != strlen(rule->newstate))
			{
				printf("Invalid rule \"%s->%s\": lenghts differ; skipping\n",
					rule->oldstate, rule->newstate);
				continue;
			}

			/* Вероятность, по умолчанию 1 */
			str = xmlGetProp(node, (xmlChar*)"probability");
			if(!str)
				p = 1;
			else
				p = atof((char*)str);
			/* Храним не в виде исходных вероятностей, а в виде границ
			под-интервалов единичного интервала */
			tmprule = automaton->rules;
			similar_rule = NULL;
			for(;;)
			{
				if(!tmprule)
					break;
				if(strcmp(tmprule->oldstate, rule->oldstate) == 0)
					similar_rule = tmprule;
				tmprule = tmprule->next;
			}
			if(similar_rule)
			{
				rule->probability = similar_rule->probability + p;
				if(rule->probability > 1.0)
				{
					printf("Total probability for rules starting with state\
\"%s\" %f>1, skipping rule \"%s->%s\"\n",
						rule->oldstate, rule->probability,
						rule->oldstate, rule->newstate);
					continue;
				}
			} else {
				rule->probability = p;
			}
			xmlFree(str);

			/* Добавить в список правил */
			rule->next = NULL;
			if(automaton->rules)
				last_rule->next = rule;
			else
				automaton->rules = rule;
			last_rule = rule;
		}
		else if(xmlStrcmp(node->name, (xmlChar*)"cell") == 0) /* Клетка */
		{
			str = xmlGetProp(node, (xmlChar*)"x");
			if(!str)
				continue;
			x = atoi((char*)str);
			xmlFree(str);

			str = xmlGetProp(node, (xmlChar*)"y");
			if(!str)
				continue;
			y = atoi((char*)str);
			xmlFree(str);

			str = xmlGetProp(node, (xmlChar*)"state");
			if(!str)
				CELL(automaton, x, y) = automaton->zerostate;
			else
				CELL(automaton, x, y) = str[0];
			xmlFree(str);
		}
	}

	/* Найти максимальную длину правила */
	automaton->maxlength = 0;
	rule = automaton->rules;
	while(rule)
	{
		if(automaton->maxlength < strlen(rule->oldstate))
			automaton->maxlength = strlen(rule->oldstate);

		rule = rule->next;
	}

	return automaton;
}

static char* newlattice = NULL;
static size_t newlattice_size = 0;

void delete_automaton(struct Automaton** a)
{
	struct Rule *r, *rnext = NULL;

	struct Automaton* automaton = *a;
	if(!automaton)
		return;
	*a = NULL;

	/* Удалить список правил */
	for(r = automaton->rules; r != NULL; r = rnext)
	{
		rnext = r->next;
		free(r);
	}

	/* Удалить поле */
	free(automaton->lattice);
	free(automaton);

	/* На всякий случай удалить временный буфер */
	if(newlattice_size != 0)
		free(newlattice);
}

void tick(struct Automaton* automaton)
{
	int i, j;
	size_t m, n;
	double rnd;
	struct Rule *next, *curr;

	size_t len;
	int will_apply;

	if(!automaton->rules)
		return;

	automaton->ticks++;

	/* Создать новое поле, если нужно */
	if(newlattice_size < automaton->size)
	{
		if(newlattice)
			free(newlattice);
		newlattice = (char*)malloc(newlattice_size = automaton->size);
	}
	memcpy(newlattice, automaton->lattice, automaton->size);

	if(rand() > RAND_MAX / 2)
	{
		/* Разбиваем поле горизонтально */
		for(i = 0; i < automaton->height; i++)
		{
			/* ...и в каждой строчке... */
			for(j = 0; j < automaton->width;)
			{
				/* ...для каждого "кирпичика" случайной длины...*/
				len = 1;
				while((double)rand() / RAND_MAX < automaton->omega)
					len++;
				if((int)len + j > automaton->width)
					len = automaton->width - j;

				/* ...найти подходящие правила... */
				rnd = (double)rand() / RAND_MAX;
				curr = automaton->rules;
				for(;;)
				{
					next = curr->next;
					if(strlen(curr->oldstate) == len)
					{
						will_apply = 1;
						for(m = 0; m < len; m++)
							if(curr->oldstate[m] != CELL(automaton, j+m, i))
							{
								will_apply = 0;
								break;
							}

						if(will_apply)
						{
							/* ... выбрать одно правило в соответствии
							с вероятностями... */
							if(rnd < curr->probability)
							{
								/* ...и применить его. */
								for(n = 0; n < len; n++)
									newlattice[i*automaton->width+j+n] =
										curr->newstate[n];
								break;
							}
						}
					}
					curr = next;
					if(!curr)
						break;
				}
				j += len;
			}
		}
	} else /* rand() > RAND_MAX / 2 */
	{
		/* Разбиваем поле вертикально */
		for(j = 0; j < automaton->width; j++)
		{
			/* ...и в каждом столбце... */
			for(i = 0; i < automaton->height;)
			{
				/* ...для каждого "кирпичика" случайной длины...*/
				len = 1;
				while((double)rand() / RAND_MAX < automaton->omega)
					len++;
				if((int)len + i > automaton->height)
					len = automaton->height - i;

				/* ...найти подходящие правила... */
				rnd = (double)rand() / RAND_MAX;
				curr = automaton->rules;
				for(;;)
				{
					next = curr->next;
					if(strlen(curr->oldstate) == len)
					{
						will_apply = 1;
						for(m = 0; m < len; m++)
							if(curr->oldstate[m] != CELL(automaton, j, i+m))
							{
								will_apply = 0;
								break;
							}

						if(will_apply)
						{
							/* ... выбрать одно правило в соответствии
							с вероятностями... */
							if(rnd < curr->probability)
							{
								/* ...и применить его. */
								for(n = 0; n < len; n++)
									newlattice[(i+n)*automaton->width+j] =
										curr->newstate[n];
								break;
							}
						}
					}
					curr = next;
					if(!curr)
						break;
				}
				i += len;
			}
		}
	}

	/* Новое поле */
	memcpy(automaton->lattice, newlattice, automaton->size);
}

