/*
 * iucvtty / iucvconn - IUCV Terminal Applications
 *
 * Configuration structure and command line processing function
 *
 * Copyright IBM Corp. 2008
 * Author(s): Hendrik Brueckner <brueckner@linux.vnet.ibm.com>
 */
#ifndef __IUCVTTY_CONFIG_H_
#define __IUCVTTY_CONFIG_H_

enum iucvterm_prg {
	PROG_IUCV_TTY  = 0,
	PROG_IUCV_CONN = 1,
};

struct iucvtty_cfg {
	char		client_check;	/* Flag to check for allowed clients.*/
	char		client_re[129];	/* Regexp to match incoming clients. */
	char		host[9];	/* IUCV target host name. */
	char		service[9];	/* IUCV service name. */
	char		**cmd_parms;	/* ptr to commandline parms. */
	char		*sessionlog;	/* ptr to session log file path. */
	unsigned char	esc_char;	/* Escape character. */
};


extern void parse_options(enum iucvterm_prg, struct iucvtty_cfg *,
			  int, char **);

#endif /* __IUCVTTY_CONFIG_H_ */
