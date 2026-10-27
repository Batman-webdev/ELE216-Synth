#include <stdio.h>
#include <stdlib.h>

#include "filtre.h"
#include "../Unity/unity.h"

void setUp(void)
{
    // This function is called before each test case
}

void tearDown(void)
{
    // This function is called after each test case
}

void test_signal_constant(void)
{
    filtre_t *f = filtre_creer(20000, 50);
    float *buffer = malloc(5 * sizeof(float));
    for (int i = 0; i < 5; i++)
    {
        buffer[i] = 5.0f;
    }

    filtre_appliquer(f, buffer, 5);
    TEST_ASSERT_EQUAL_FLOAT(5.0f, buffer[4]);
    filtre_detruire(f);
    free(buffer);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_signal_constant);
    UNITY_END();
}
