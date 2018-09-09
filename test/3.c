#include <stdio.h>
__attribute__((noinline))
int addNumbers(int n)
{
    int z = 15;
    int v = ++z;
    if(n != 0)
	z = v + z + n;

    --z;
    z = n * z;

    return z;
}

int main()
{
    int num;
    printf("Enter a positive integer: ");
    scanf("%d", &num);
    printf("Sum = %d",addNumbers(num));
    return 0;
}


