/*
 * Copyright (c) 2026 ykat UG (haftungsbeschraenkt) - All Rights Reserved
 *
 * Permission for non-commercial use is hereby granted,
 * free of charge, without warranty of any kind.
 */

#include <stdio.h>

void test_htable_list(void);
void test_htable_table(void);
void test_swar(void);

int main(void)
{
	test_htable_list();
	test_htable_table();
	test_swar();
	puts("htable tests passed");
	return 0;
}