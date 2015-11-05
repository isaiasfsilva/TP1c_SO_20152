#include <unistd.h>

int main() {
	const char* file1 = "./test1";
	const char* file2 = "./test2";
	const char* file3 = "./test3";
	const char* file4 = "./test4";

	pid_t p1, p2, p3, p4;
	int a;

	p1 = fork();
	if(p1 == 0 ) {
		execvp(&file1[0], &file1);
	} else if(p1 > 0) {
		p2 = fork();
		if(p2 == 0) {
			execvp(&file2[0], &file2);
		} else if(p2 > 0) {
			p3 = fork();
			if(p3 == 0) {
				execvp(&file3[0], &file3);
			} else if (p3 > 0)  {
				p4 = fork();
				if(p4 == 0) {
					execvp(&file4[0], &file4);
				} else if(p4 > 0) {
					wait(p1);
					wait(p2);
					wait(p3);
					wait(p4);	
				}
			}
		}
	}

	return 0;
}