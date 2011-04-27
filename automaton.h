
#ifndef _AUTOMATON_H_
#define _AUTOMATON_H_

/** Правило клеточного автомата
**/
struct Rule
{
#if 0
	/** Максимальный размер окружения для данной размерности пространства **/
	#define NEIGHBOURHOOD_SIZE 8

	/** Соседи на текущем шаге**/
	char curr_neighbourhood[NEIGHBOURHOOD_SIZE];

	/** Соседи на следующем шаге**/
	char new_neighbourhood[NEIGHBOURHOOD_SIZE];
#endif  /* 0 */

	/** Цепочка до и после подстановки **/
	char *oldstate, *newstate;

	/** Вероятность применения правила **/
	double probability;

	/** Указатель на следующее правило в списке (NULL, если последнее) **/
	struct Rule* next;
};

/** Клеточный автомат (двумерный, недетерминированный)
**/
typedef struct
{
	/** Высота и ширина поля **/
	int width, height;

	/** Число состояний (без нулевого) **/
	int nstates;

	/** Код нулевого состояния **/
	char zerostate;

	/* Название состояния - один алфавитно-цифровой символ, регистр важен,
	    итого максимум 62 состояния */
	#define MAX_STATES 62

	/** Коды обычных состояний **/
	char states[MAX_STATES];

	/** Правила **/
	struct Rule* rules;

	/** Поле **/
	char** lattice;

	/** Количество прошедших квантов времени **/
	int ticks;

} Automaton;


/** Загрузка клеточного автомата из файла
 *   @param filename имя файла
 *   @return автомат
**/
extern Automaton* load_automaton(const char* filename);

/** Освобождение ресурсов, занятых автоматом
 *   @param a адрес клеточного автомата для удаления
**/
extern void delete_automaton(Automaton** a);

/** Применение правил к клеткам автомата
 *   @param aвтомам
**/
extern void tick(Automaton* automaton);


#endif  /* _AUTOMATON_H_ */

