/*
 *
 *   Copyright (c) International Business Machines  Corp., 2001
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/*
 * NAME
 *	writev05.c
 *
 * DESCRIPTION
 *	These testcases are written to test writev() on sparse files. This
 *	is same as writev02.c. But the initial write() with valid data is
 *	done at the beginning of the file.
 *
 * USAGE:  <for command-line>
 *      writev05 [-c n] [-e] [-i n] [-I x] [-P x] [-t]
 *      where,  -c n : Run n copies concurrently.
 *              -e   : Turn on errno logging.
 *              -i n : Execute test n times.
 *              -I x : Execute test for x seconds.
 *              -P x : Pause for x seconds between iterations.
 *              -t   : Turn on syscall timing.
 *
 * History
 *	07/2001 John George
 *		-Ported
 *      04/2002 wjhuie sigset cleanups
 *
 * Restrictions
 *	NONE
 */

#include <sys/types.h>
#include <signal.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <memory.h>
#include <errno.h>
#include "test.h"
#include <sys/mman.h>

#define	K_1	8192

#define	NBUFS		2
#define	CHUNK		K_1	/* single chunk */
#define	MAX_IOVEC	2
#define	DATA_FILE	"writev_data_file"

char buf1[K_1];
char buf2[K_1];
char buf3[K_1];

char *bad_addr = 0;

struct iovec wr_iovec[MAX_IOVEC] = {
	{(caddr_t) - 1, CHUNK},
	{NULL, 0}
};

char name[K_1], f_name[K_1];
int fd[2], in_sighandler;
char *buf_list[NBUFS];

char *TCID = "writev05";
int TST_TOTAL = 1;

void sighandler(int);
long l_seek(int, long, int);
void setup(void);
void cleanup(void);

int main(int argc, char **argv)
{
	int lc;

	int nbytes;

	tst_parse_opts(argc, argv, NULL, NULL);

	setup();		/* set "tstdir", and "testfile" vars */

	/* The following loop checks looping state if -i option given */
	for (lc = 0; TEST_LOOPING(lc); lc++) {

		/* reset tst_count in case we are looping */
		tst_count = 0;

		buf_list[0] = buf1;
		buf_list[1] = buf2;

		fd[1] = -1;	/* Invalid file descriptor */

		if (signal(SIGTERM, sighandler) == SIG_ERR) {
			perror("signal");
			tst_resm(TFAIL, "signal() SIGTERM FAILED");
			cleanup();
		}

		if (signal(SIGPIPE, sighandler) == SIG_ERR) {
			perror("signal");
			tst_resm(TFAIL, "signal() SIGPIPE FAILED");
			cleanup();
		}

		/* Fill the buf_list[0] and buf_list[1] with 0 zeros */
		memset(buf_list[0], 0, K_1);
		memset(buf_list[1], 0, K_1);

		if ((fd[0] = open(f_name, O_WRONLY | O_CREAT, 0666)) < 0) {
			tst_resm(TFAIL, "open(2) failed: fname = %s, "
				 "errno = %d", f_name, errno);
			cleanup();
		} else {
			if ((nbytes = write(fd[0], buf_list[1], K_1)) != K_1) {
				tst_resm(TFAIL, "write(2) failed: nbytes "
					 "= %d, errno = %d", nbytes, errno);
				cleanup();
			}
		}

		if (close(fd[0]) < 0) {
			tst_resm(TFAIL, "close failed: errno = %d", errno);
			cleanup();
		}

		if ((fd[0] = open(f_name, O_RDWR, 0666)) < 0) {
			tst_resm(TFAIL, "open failed: fname = %s, errno = %d",
				 f_name, errno);
			cleanup();
		}

		/*
		 * In this block we are trying to call writev() with invalid
		 * vector to be written in a sparse file. This will return
		 * EFAULT. At the same time, check should be made whether
		 * the scheduled write() with valid data is done correctly
		 * or not.
		 */
		l_seek(fd[0], 0, 0);
		TEST(writev(fd[0], wr_iovec, 2));
		if (TEST_RETURN < 0) {
			if (TEST_ERRNO == EFAULT) {
				tst_resm(TPASS, "Received EFAULT as expected");
			} else {
				tst_resm(TFAIL, "Expected EFAULT, got %d",
					 TEST_ERRNO);
			}
			l_seek(fd[0], K_1, 0);
			if ((nbytes = read(fd[0], buf_list[0], CHUNK)) != 0) {
				tst_resm(TFAIL, "Expected nbytes = 0, got "
					 "%d", nbytes);
			}
		} else {
			tst_resm(TFAIL, "Error writev returned a positive "
				 "value");
		}
	}
	close(fd[0]);
	close(fd[1]);
	cleanup();
	tst_exit();

}

/*
 * setup()
 *	performs all ONE TIME setup for this test
 */
void setup(void)
{

	tst_sig(FORK, DEF_HANDLER, cleanup);

	/* Pause if that option was specified.
	 * TEST_PAUSE contains the code to fork the test with the -i option.
	 * You want to make sure you do this before you create your temporary
	 * directory.
	 */
	TEST_PAUSE;

	/* Create a unique temporary directory and chdir() to it. */
	tst_tmpdir();

	strcpy(name, DATA_FILE);
	sprintf(f_name, "%s.%d", name, getpid());

	bad_addr = mmap(0, 1, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	if (bad_addr == MAP_FAILED) {
		printf("mmap failed\n");
	}
	wr_iovec[0].iov_base = bad_addr;

}

/*
 * cleanup()
 *	performs all ONE TIME cleanup for this test at
 *	completion or premature exit
 */
void cleanup(void)
{

	if (unlink(f_name) < 0) {
		tst_resm(TFAIL, "unlink Failed--file = %s, errno = %d",
			 f_name, errno);
	}
	tst_rmdir();

}

/*
 * sighandler()
 *	Signal handler for SIGTERM and SIGPIPE
 */
void sighandler(int sig)
{
	switch (sig) {
	case SIGTERM:
		break;
	case SIGPIPE:
		++in_sighandler;
		return;
	default:
		tst_resm(TFAIL, "sighandler() received invalid signal "
			 ": %d", sig);
		break;
	}

	if ((unlink(f_name) < 0) && (errno != ENOENT)) {
		tst_resm(TFAIL, "unlink Failed--file = %s, errno = %d",
			 f_name, errno);
		cleanup();
	}
	exit(sig);
}

/*
 * l_seek()
 *	Wrap around for regular lseek() to give error message on failure
 */
long l_seek(int fdesc, long offset, int whence)
{
	if (lseek(fdesc, offset, whence) < 0) {
		tst_resm(TFAIL, "lseek Failed : errno = %d", errno);
	}
	return 0;
}
