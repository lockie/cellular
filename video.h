
#ifndef _VIDEO_H_
#define _VIDEO_H_

#include "automaton.h"


/** Открыть на запись выходной видеофайл
 *   @param automaton клеточный автомат, который будет выводиться в файл
 *   @param filename имя файла
 *   @return внутренняя структура, отвечающая за видеофайл
**/
extern void* open_renderer(Automaton* automaton, const char* filename);

/** Записать кадр с состоянием автомата в видеофайл
 *   @param renderer выходной видеофайл, возвращённый функцией open_renderer
 *   @param automaton клеточный автомат
 *   @param text текст для вывода или NULL, если не нужно
**/
extern void render(void* renderer, Automaton* automaton, const char* text);

/** Закрыть выходной видеофайл
 *   @param renderer выходной видеофайл, возвращённый функцией open_renderer
 * **/
extern void close_renderer(void* renderer);

#endif  /* _VIDEO_H_ */

