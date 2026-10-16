#include "revert_string.h"
#include <string.h>

void RevertString(char *str)
{
	size_t length;
	size_t left;
	size_t right;
	char help;

	length = strlen(str);

	if (length < 2){
		return;
	}
	left = 0 ;
	right = length -1;

	while (left < right){
		help = str[left];
		str[left] = str[right];
		str[right] = help;

		left = left + 1;
		right = right -1;
	}
}

