#ifndef APPS_SHEET_H
#define APPS_SHEET_H

/*
 * sheet.h:
 * Milestone 6 Mini Spreadsheet Application (Excel-like).
 *
 * Why this is needed:
 * Provides a 2D grid of 60 cells (6 columns A-F x 10 rows 1-10) with
 * cell selection, formula bar, arithmetic evaluation, range functions
 * (=SUM, =AVG, =MIN, =MAX), and live automatic recalculation.
 */

/* Launches or brings to front the Spreadsheet application window */
void app_sheet_open(void);

#endif /* APPS_SHEET_H */
