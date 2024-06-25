#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(main);

int main(void)
{
	LOG_INF("Tutorial - The Basics");
	while (1) {
		k_sleep(K_SECONDS(10));
	}
	return 0;
}