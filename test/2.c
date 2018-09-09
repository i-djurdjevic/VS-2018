#include <stdio.h>
__attribute__((noinline))
int addNumbers(int n)
{
    int z = 15;
    if(n != 0)
        return n + z;
    else
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


