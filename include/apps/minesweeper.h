#ifndef APPS_MINESWEEPER_H
#define APPS_MINESWEEPER_H

/*
 * minesweeper.h:
 * Classic Windows Minesweeper desktop puzzle application for MyOS.
 *
 * Why this is needed:
 * Provides the classic 9x9 grid, 10-mine strategic deduction game for MyOS.
 * Implements full game loop including recursive zero-mine flood fill,
 * flag placement, 7-segment digital mine and timer displays, animated
 * yellow smiley face button, and authentic Windows 95 beveled styling.
 */

/*
 * app_minesweeper_open:
 * Creates and displays the Minesweeper window or brings existing instance to front.
 */
void app_minesweeper_open(void);

#endif /* APPS_MINESWEEPER_H */
