#include <twz/obj.h>
#include <twz/queue.h>

#include <unordered_map>

int main()
{
	fprintf(stderr, "constructing map\n");
	std::unordered_map<std::string, int> m;
	fprintf(stderr, "append map\n");
	m["foo"] = 32;
	printf("Hello! %d\n", m["foo"]);
}
