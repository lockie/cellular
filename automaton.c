
#include <string.h>
#include <float.h>
#include <math.h>

#include <libxml/parser.h>

#include "automaton.h"


struct Automaton* load_automaton(const char* filename)
{
	int i;
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
	automaton->lattice = (char**)malloc(automaton->height * sizeof(char*));
	for(i = 0; i < automaton->height; i++)
		automaton->lattice[i] = (char*)malloc(automaton->width * sizeof(char));

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
			for(i = 0; i < automaton->height; i++)
				memset(automaton->lattice[i], automaton->zerostate,
					automaton->width);
		}
		else if(xmlStrcmp(node->name, (xmlChar*)"state") == 0) /* Обычное состояние */
		{
			str = xmlGetProp(node, (xmlChar*)"name");
			if(!str)
			{
				automaton->states[automaton->nstates] =
					automaton->nstates + 48; /* ASCII HACK */
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
			/* TODO : диагностику на предмет того, имеют ли состояния
			одну и ту же длину*/

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
				/* TODO : диагностику на предмет того, что суммарная вероятность
				/  нескольких правил с одинаковыми левыми частями больше 1 */
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
				automaton->lattice[y][x] = automaton->zerostate;
			else
				automaton->lattice[y][x] = str[0];
			xmlFree(str);
		}
	}
	return automaton;
}

void delete_automaton(struct Automaton** a)
{
	int i;
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
	for(i = 0; i < automaton->height; i++)
		free(automaton->lattice[i]);
	free(automaton->lattice);

	free(automaton);
}

void tick(struct Automaton* automaton)
{
	int i, j;
	size_t m, n;
	double rnd;
	struct Rule *next, *curr;

	char** newlattice;
	size_t len, maxlen = 0;
	int will_apply;

	printf("Tick %d\n", automaton->ticks++);

	if(!automaton->rules)
		return;

	/* Найти максимальную длину правила. TODO : инвариант */
	curr = automaton->rules;
	for(;;)
	{
		if(maxlen < strlen(curr->oldstate))
			maxlen = strlen(curr->oldstate);

		curr = curr->next;
		if(!curr)
			break;
	}

	/* Создать новое поле */
	newlattice = (char**)malloc(automaton->height * sizeof(char*));
	for(i = 0; i < automaton->height; i++)
	{
		newlattice[i] = (char*)malloc(automaton->width * sizeof(char));
		memcpy(newlattice[i], automaton->lattice[i], automaton->width);
	}

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
							if(curr->oldstate[m] != automaton->lattice[i][j+m])
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
									newlattice[i][j+n] = curr->newstate[n];
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
							if(curr->oldstate[m] != automaton->lattice[i+m][j])
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
									newlattice[i+n][j] = curr->newstate[n];
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
	for(i = 0; i < automaton->height; i++)
		free(automaton->lattice[i]);
	free(automaton->lattice);
	automaton->lattice = newlattice;
}

