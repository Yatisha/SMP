Make : cli_application.c smp_api.c server_application.c
	gcc -o cli cli_application.c smp_api.c
	gcc -o ser server_application.c smp_api.c
	gcc -o eppm ep_port_mapper.c
	
	
