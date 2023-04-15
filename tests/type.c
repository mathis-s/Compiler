

int (*func(int a))[10]
{
    return (int (*)[10]) 0xf00f;
}

int main (int i)
{
    int (*(*b)(int))[10] = func;


    return (*b(0))[9];
}

void func2 (int* const a)
{
    //a++;
    (*a)++;
}

void func3 (const int* a)
{
    a++;
    //(*a)++;
}

enum ABC {A,B,C};
typedef union {int a; int b;} PairU;
typedef struct Pair {int a; int b;} Pair_t;

int func4 ()
{
    const int a = 1 * 3 * 100;
    
    struct Pair pair = {42, 43};
    PairU pairU;

    pairU.a = 1;

    enum ABC d = C;

    return pair.a + pairU.b + a + d + B;
}


int func5 ()
{
    struct {int x; int y;} coords[2] = 
    {
        {1, 2},
        {4, 5},
    };

    int sum = 0;
    for (int i = 0; i < 2; i++)
        sum += coords[i].x * coords[i].y;
    return sum;
}