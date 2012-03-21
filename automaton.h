
#ifndef _AUTOMATON_H_
#define _AUTOMATON_H_

/** Правило клеточного автомата
**/
struct Rule
{
	/** Цепочка до и после подстановки **/
	char *oldstate, *newstate;

	/** Вероятность применения правила **/
	double probability;

	/** Указатель на следующее правило в списке (NULL, если последнее) **/
	struct Rule* next;
};

/** Макрос для доступа к клетке с заданным индексом
 **/
#define CELL(automaton, x, y) automaton->lattice[(y)*automaton->width+x]

/** Клеточный автомат (двумерный, недетерминированный)
**/
struct Automaton
{
	/** Высота и ширина поля **/
	int width, height;

	/** Размер поля в байтах **/
	size_t size;

	/** Число состояний (без нулевого) **/
	int nstates;

	/** Код нулевого состояния **/
	char zerostate;

	/* Название состояния - один алфавитно-цифровой символ, регистр важен,
	    итого максимум 62 состояния */
	#define MAX_STATES 62

	/** Параметр \omega **/
	double omega;

	/** Коды обычных состояний **/
	char states[MAX_STATES];

	/** Правила **/
	struct Rule* rules;

	/** Максимальная длина правила **/
	size_t maxlength;

	/** Поле **/
	char* lattice;

	/** Количество прошедших квантов времени **/
	int ticks;
};


/** Загрузка клеточного автомата из файла
 *   @param filename имя файла
 *   @return автомат
**/
extern struct Automaton* load_automaton(const char* filename);

/** Освобождение ресурсов, занятых автоматом
 *   @param a адрес клеточного автомата для удаления
**/
extern void delete_automaton(struct Automaton** a);

/** Применение правил к клеткам автомата
 *   @param aвтомам
**/
extern void tick(struct Automaton* automaton);


#endif  /* _AUTOMATON_H_ */

