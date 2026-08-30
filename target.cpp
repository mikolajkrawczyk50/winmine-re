#include <windows.h>
#include <stdio.h>

extern "C" __declspec(dllexport) __attribute__((noinline)) int calculate_price(int item_id, int base_price, int tax_rate) {
    int total = base_price + (base_price * tax_rate / 100);
    printf("[TARGET] calculate_price(item=%d, base=%d, tax=%d%%) => total=%d\n", item_id, base_price, tax_rate, total);
    fflush(stdout);
    return total;
}

extern "C" __declspec(dllexport) __attribute__((noinline)) int execute_job(int job_id, const char* job_name) {
    printf("[TARGET] execute_job(id=%d, name='%s') running...\n", job_id, job_name);
    fflush(stdout);
    return job_id * 10;
}

int main() {
    printf("Target running. PID: %lu\n", GetCurrentProcessId());
    printf("Waiting for injection...\n");
    fflush(stdout);
    int iter = 1;
    while (1) {
        printf("--- Loop Iteration %d ---\n", iter);
        int price = calculate_price(100 + iter, 500, 20);
        printf("[TARGET RESULT] price: %d\n", price);

        int job_ret = execute_job(iter, "WorkerTask");
        printf("[TARGET RESULT] job_ret: %d\n", job_ret);
        fflush(stdout);

        iter++;
        Sleep(1000);
    }
    return 0;
}
