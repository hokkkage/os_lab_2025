#include "swap.h"

void Swap(char *left, char *right)
{
	char help;
	help = *left;
	*left = *right;
	*right = help;
}
