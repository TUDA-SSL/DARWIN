#include "afl_darwin.h"
#include "stdio.h"
#include "stdint.h"

int main(int argc, char *argv[]) {
	DARWIN_init(2u, 15);

	for (uint64_t i = 0; i < 1000; i++) {
		int op_tmp = DARWIN_SelectOperator(0u);
		DARWIN_NotifyFeedback(0u, 1u);
		printf("%d\n", op_tmp);
	}

	int op_2;
        for (uint64_t i = 0; i < 999; i++) {
		op_2 = DARWIN_SelectOperator(1u);
		DARWIN_NotifyFeedback(1u,i);
	}

//	printf("%d, %d\n", op_1, op_2);

	return 0;
}
