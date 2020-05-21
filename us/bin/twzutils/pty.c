#include <twz/gate.h>
#include <twz/pty.h>

TWZ_GATE(pty_read_server, PTY_GATE_READ_SERVER);
TWZ_GATE(pty_write_server, PTY_GATE_WRITE_SERVER);
TWZ_GATE(pty_read_client, PTY_GATE_READ_CLIENT);
TWZ_GATE(pty_write_client, PTY_GATE_WRITE_CLIENT);
TWZ_GATE(pty_ioctl_server, PTY_GATE_IOCTL_SERVER);
TWZ_GATE(pty_ioctl_client, PTY_GATE_IOCTL_CLIENT);
TWZ_GATE(pty_poll_server, PTY_GATE_POLL_SERVER);
TWZ_GATE(pty_poll_client, PTY_GATE_POLL_CLIENT);

int main()
{
	__builtin_unreachable();
}
