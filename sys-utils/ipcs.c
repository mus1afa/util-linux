/* Original author unknown, may be "krishna balasub@cis.ohio-state.edu" */
/*
 * Modified Sat Oct  9 10:55:28 1993 for 0.99.13
 *
 * Patches from Mike Jagdis (jaggy@purplet.demon.co.uk) applied Wed Feb 8
 * 12:12:21 1995 by faith@cs.unc.edu to print numeric uids if no passwd file
 * entry.
 *
 * Patch from arnolds@ifns.de (Heinz-Ado Arnolds) applied Mon Jul 1 19:30:41
 * 1996 by janl@math.uio.no to add code missing in case PID: clauses.
 *
 * Patched to display the key field -- hy@picksys.com 12/18/96
 *
 * 1999-02-22 Arkadiusz Mi�kiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 */

#include <errno.h>
#include <features.h>
#include <getopt.h>

#include "c.h"
#include "nls.h"
#include "closestream.h"

#include "ipcutils.h"


#define LIMITS 1
#define STATUS 2
#define CREATOR 3
#define TIME 4
#define PID 5

void do_shm (char format);
void do_sem (char format);
void do_msg (char format);
void print_shm (int id);
void print_msg (int id);
void print_sem (int id);

static void __attribute__ ((__noreturn__)) usage(FILE * out)
{
	fprintf(out, USAGE_HEADER);
	fprintf(out, " %s [resource ...] [output-format]\n", program_invocation_short_name);
	fprintf(out, " %s [resource] -i <id>\n", program_invocation_short_name);
	fprintf(out, USAGE_OPTIONS);
	fputs(_(" -i, --id <id>  print details on resource identified by id\n"), out);
	fprintf(out, USAGE_HELP);
	fprintf(out, USAGE_VERSION);
	fputs(_("\n"), out);
	fputs(_("Resource options:\n"), out);
	fputs(_(" -m, --shmems      shared memory segments\n"), out);
	fputs(_(" -q, --queues      message queues\n"), out);
	fputs(_(" -s, --semaphores  semaphores\n"), out);
	fputs(_(" -a, --all         all (default)\n"), out);
	fputs(_("\n"), out);
	fputs(_("Output format:\n"), out);
	fputs(_(" -t, --time        show attach, detach and change times\n"), out);
	fputs(_(" -p, --pid         show creator and last operations PIDs\n"), out);
	fputs(_(" -c, --creator     show creator and owner\n"), out);
	fputs(_(" -l, --limits      show resource limits\n"), out);
	fputs(_(" -u, --summary     show status summary\n"), out);
	fprintf(out, USAGE_MAN_TAIL("ipcs(1)"));
	exit(out == stderr ? EXIT_FAILURE : EXIT_SUCCESS);
}

int main (int argc, char **argv)
{
	int opt, msg = 0, sem = 0, shm = 0, id=0, print=0;
	char format = 0;
	static const struct option longopts[] = {
		{"id", required_argument, NULL, 'i'},
		{"shmems", no_argument, NULL, 'm'},
		{"queues", no_argument, NULL, 'q'},
		{"semaphores", no_argument, NULL, 's'},
		{"all", no_argument, NULL, 'a'},
		{"time", no_argument, NULL, 't'},
		{"pid", no_argument, NULL, 'p'},
		{"creator", no_argument, NULL, 'c'},
		{"limits", no_argument, NULL, 'l'},
		{"summary", no_argument, NULL, 'u'},
		{"version", no_argument, NULL, 'V'},
		{"help", no_argument, NULL, 'h'},
		{NULL, 0, NULL, 0}
	};
	char options[] = "i:mqsatpcluVh";

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);
	atexit(close_stdout);

	while ((opt = getopt_long(argc, argv, options, longopts, NULL)) != -1) {
		switch (opt) {
		case 'i':
			id = atoi (optarg);
			print = 1;
			break;
		case 'a':
			msg = shm = sem = 1;
			break;
		case 'q':
			msg = 1;
			break;
		case 's':
			sem = 1;
			break;
		case 'm':
			shm = 1;
			break;
		case 't':
			format = TIME;
			break;
		case 'c':
			format = CREATOR;
			break;
		case 'p':
			format = PID;
			break;
		case 'l':
			format = LIMITS;
			break;
		case 'u':
			format = STATUS;
			break;
		case 'h':
			usage(stdout);
		case 'V':
			printf(UTIL_LINUX_VERSION);
			return EXIT_SUCCESS;
		default:
			usage(stderr);
		}
	}

	if  (print) {
		if (shm)
			print_shm (id);
		if (sem)
			print_sem (id);
		if (msg)
			print_msg (id);
		if (!shm && !sem && !msg )
			usage (stderr);
	} else {
		if ( !shm && !msg && !sem)
			msg = sem = shm = 1;
		printf ("\n");

		if (shm) {
			do_shm (format);
			printf ("\n");
		}
		if (sem) {
			do_sem (format);
			printf ("\n");
		}
		if (msg) {
			do_msg (format);
			printf ("\n");
		}
	}
	return EXIT_SUCCESS;
}

static void print_perms (int id, struct ipc_perm *ipcp)
{
	struct passwd *pw;
	struct group *gr;

	printf ("%-10d %-10o", id, ipcp->mode & 0777);

	if ((pw = getpwuid(ipcp->cuid)))
		printf(" %-10s", pw->pw_name);
	else
		printf(" %-10u", ipcp->cuid);
	if ((gr = getgrgid(ipcp->cgid)))
		printf(" %-10s", gr->gr_name);
	else
		printf(" %-10u", ipcp->cgid);

	if ((pw = getpwuid(ipcp->uid)))
		printf(" %-10s", pw->pw_name);
	else
		printf(" %-10u", ipcp->uid);
	if ((gr = getgrgid(ipcp->gid)))
		printf(" %-10s\n", gr->gr_name);
	else
		printf(" %-10u\n", ipcp->gid);
}

void do_shm (char format)
{
	int maxid, shmid, id;
	struct shmid_ds shmseg;
	struct shm_info shm_info;
	struct ipc_perm *ipcp = &shmseg.shm_perm;
	struct passwd *pw;
	struct ipc_limits lim;

	maxid = shmctl (0, SHM_INFO, (struct shmid_ds *) (void *) &shm_info);
	if (maxid < 0) {
		printf (_("kernel not configured for shared memory\n"));
		return;
	}

	switch (format) {
	case LIMITS:
		printf (_("------ Shared Memory Limits --------\n"));
		if (ipc_shm_get_limits(&lim))
			return;
		printf (_("max number of segments = %ju\n"), lim.shmmni);
		printf (_("max seg size (kbytes) = %ju\n"), lim.shmmax / 1024);
		printf (_("max total shared memory (kbytes) = %ju\n"),
					(lim.shmall / 1024) * getpagesize());
		printf (_("min seg size (bytes) = %ju\n"), lim.shmmin);
		return;

	case STATUS:
		printf (_("------ Shared Memory Status --------\n"));
		/*
		 * TRANSLATORS: This output format is maintained for backward
		 * compatibility as ipcs is used in scripts. For consistency
		 * with the rest, the translated form can follow this model:
		 *
		 * "segments allocated = %d\n"
		 * "pages allocated = %ld\n"
		 * "pages resident = %ld\n"
		 * "pages swapped = %ld\n"
		 * "swap performance = %ld attempts, %ld successes\n"
		 */
		printf (_("segments allocated %d\n"
			  "pages allocated %ld\n"
			  "pages resident  %ld\n"
			  "pages swapped   %ld\n"
			  "Swap performance: %ld attempts\t %ld successes\n"),
			shm_info.used_ids,
			shm_info.shm_tot,
			shm_info.shm_rss,
			shm_info.shm_swp,
			shm_info.swap_attempts, shm_info.swap_successes);
		return;

	case CREATOR:
		printf (_("------ Shared Memory Segment Creators/Owners --------\n"));
		printf ("%-10s %-10s %-10s %-10s %-10s %-10s\n",
			_("shmid"),_("perms"),_("cuid"),_("cgid"),_("uid"),_("gid"));
		break;

	case TIME:
		printf (_("------ Shared Memory Attach/Detach/Change Times --------\n"));
		printf ("%-10s %-10s %-20s %-20s %-20s\n",
			_("shmid"),_("owner"),_("attached"),_("detached"),
			_("changed"));
		break;

	case PID:
		printf (_("------ Shared Memory Creator/Last-op PIDs --------\n"));
		printf ("%-10s %-10s %-10s %-10s\n",
			_("shmid"),_("owner"),_("cpid"),_("lpid"));
		break;

	default:
		printf (_("------ Shared Memory Segments --------\n"));
		printf ("%-10s %-10s %-10s %-10s %-10s %-10s %-12s\n",
			_("key"),_("shmid"),_("owner"),_("perms"),_("bytes"),
			_("nattch"),_("status"));
		break;
	}

	for (id = 0; id <= maxid; id++) {
		shmid = shmctl (id, SHM_STAT, &shmseg);
		if (shmid < 0)
			continue;
		if (format == CREATOR)  {
			print_perms (shmid, ipcp);
			continue;
		}
		pw = getpwuid(ipcp->uid);
		switch (format) {
		case TIME:
			if (pw)
				printf ("%-10d %-10.10s", shmid, pw->pw_name);
			else
				printf ("%-10d %-10u", shmid, ipcp->uid);
			/* ctime uses static buffer: use separate calls */
			printf(" %-20.16s", shmseg.shm_atime
			       ? ctime(&shmseg.shm_atime) + 4 : _("Not set"));
			printf(" %-20.16s", shmseg.shm_dtime
			       ? ctime(&shmseg.shm_dtime) + 4 : _("Not set"));
			printf(" %-20.16s\n", shmseg.shm_ctime
			       ? ctime(&shmseg.shm_ctime) + 4 : _("Not set"));
			break;
		case PID:
			if (pw)
				printf ("%-10d %-10.10s", shmid, pw->pw_name);
			else
				printf ("%-10d %-10u", shmid, ipcp->uid);
			printf (" %-10d %-10d\n",
				shmseg.shm_cpid, shmseg.shm_lpid);
			break;

		default:
			printf("0x%08x ",ipcp->KEY );
			if (pw)
				printf ("%-10d %-10.10s", shmid, pw->pw_name);
			else
				printf ("%-10d %-10u", shmid, ipcp->uid);
			printf (" %-10o %-10lu %-10ld %-6s %-6s\n",
				ipcp->mode & 0777,
				/*
				 * earlier: int, Austin has size_t
				 */
				(unsigned long) shmseg.shm_segsz,
				/*
				 * glibc-2.1.3 and earlier has unsigned short;
				 * Austin has shmatt_t
				 */
				(long) shmseg.shm_nattch,
				ipcp->mode & SHM_DEST ? _("dest") : " ",
				ipcp->mode & SHM_LOCKED ? _("locked") : " ");
			break;
		}
	}
	return;
}

void do_sem (char format)
{
	int maxid, semid, id;
	struct semid_ds semary;
	struct seminfo seminfo;
	struct ipc_perm *ipcp = &semary.sem_perm;
	struct passwd *pw;
	union semun arg;
	struct ipc_limits lim;

	arg.array = (ushort *)  (void *) &seminfo;
	maxid = semctl (0, 0, SEM_INFO, arg);
	if (maxid < 0) {
		printf (_("kernel not configured for semaphores\n"));
		return;
	}

	switch (format) {
	case LIMITS:
		printf (_("------ Semaphore Limits --------\n"));
		if (ipc_sem_get_limits(&lim))
			return;
		printf (_("max number of arrays = %d\n"), lim.semmni);
		printf (_("max semaphores per array = %d\n"), lim.semmsl);
		printf (_("max semaphores system wide = %d\n"), lim.semmns);
		printf (_("max ops per semop call = %d\n"), lim.semopm);
		printf (_("semaphore max value = %d\n"), lim.semvmx);
		return;

	case STATUS:
		printf (_("------ Semaphore Status --------\n"));
		printf (_("used arrays = %d\n"), seminfo.semusz);
		printf (_("allocated semaphores = %d\n"), seminfo.semaem);
		return;

	case CREATOR:
		printf (_("------ Semaphore Arrays Creators/Owners --------\n"));
		printf ("%-10s %-10s %-10s %-10s %-10s %-10s\n",
			_("semid"),_("perms"),_("cuid"),_("cgid"),_("uid"),_("gid"));
		break;

	case TIME:
		printf (_("------ Semaphore Operation/Change Times --------\n"));
		printf ("%-8s %-10s %-26.24s %-26.24s\n",
			_("semid"),_("owner"),_("last-op"),_("last-changed"));
		break;

	case PID:
		break;

	default:
		printf (_("------ Semaphore Arrays --------\n"));
		printf ("%-10s %-10s %-10s %-10s %-10s\n",
			_("key"),_("semid"),_("owner"),_("perms"),_("nsems"));
		break;
	}

	for (id = 0; id <= maxid; id++) {
		arg.buf = (struct semid_ds *) &semary;
		semid = semctl (id, 0, SEM_STAT, arg);
		if (semid < 0)
			continue;
		if (format == CREATOR)  {
			print_perms (semid, ipcp);
			continue;
		}
		pw = getpwuid(ipcp->uid);
		switch (format) {
		case TIME:
			if (pw)
				printf ("%-8d %-10.10s", semid, pw->pw_name);
			else
				printf ("%-8d %-10u", semid, ipcp->uid);
			printf ("  %-26.24s", semary.sem_otime
				? ctime(&semary.sem_otime) : _("Not set"));
			printf (" %-26.24s\n", semary.sem_ctime
				? ctime(&semary.sem_ctime) : _("Not set"));
			break;
		case PID:
			break;

		default:
			printf("0x%08x ", ipcp->KEY);
			if (pw)
				printf ("%-10d %-10.10s", semid, pw->pw_name);
			else
				printf ("%-10d %-10u", semid, ipcp->uid);
			printf (" %-10o %-10ld\n",
				ipcp->mode & 0777,
				/*
				 * glibc-2.1.3 and earlier has unsigned
				 * short. glibc-2.1.91 has variation between
				 * unsigned short and unsigned long. Austin
				 * prescribes unsigned short.
				 */
				(long) semary.sem_nsems);
			break;
		}
	}
}

void do_msg (char format)
{
	int maxid, msqid, id;
	struct msqid_ds msgque;
	struct msginfo msginfo;
	struct ipc_perm *ipcp = &msgque.msg_perm;
	struct passwd *pw;
	struct ipc_limits lim;

	maxid = msgctl (0, MSG_INFO, (struct msqid_ds *) (void *) &msginfo);
	if (maxid < 0) {
		printf (_("kernel not configured for message queues\n"));
		return;
	}

	switch (format) {
	case LIMITS:
		if (ipc_msg_get_limits(&lim))
			return;
		printf (_("------ Messages Limits --------\n"));
		printf (_("max queues system wide = %d\n"), lim.msgmni);
		printf (_("max size of message (bytes) = %zu\n"), lim.msgmax);
		printf (_("default max size of queue (bytes) = %d\n"), lim.msgmnb);
		return;

	case STATUS:
		printf (_("------ Messages Status --------\n"));
		printf (_("allocated queues = %d\n"), msginfo.msgpool);
		printf (_("used headers = %d\n"), msginfo.msgmap);
		printf (_("used space = %d bytes\n"), msginfo.msgtql);
		return;

	case CREATOR:
		printf (_("------ Message Queues Creators/Owners --------\n"));
		printf ("%-10s %-10s %-10s %-10s %-10s %-10s\n",
			_("msqid"),_("perms"),_("cuid"),_("cgid"),_("uid"),_("gid"));
		break;

	case TIME:
		printf (_("------ Message Queues Send/Recv/Change Times --------\n"));
		printf ("%-8s %-10s %-20s %-20s %-20s\n",
			_("msqid"),_("owner"),_("send"),_("recv"),_("change"));
		break;

	case PID:
		printf (_("------ Message Queues PIDs --------\n"));
		printf ("%-10s %-10s %-10s %-10s\n",
			_("msqid"),_("owner"),_("lspid"),_("lrpid"));
		break;

	default:
		printf (_("------ Message Queues --------\n"));
		printf ("%-10s %-10s %-10s %-10s %-12s %-12s\n",
			_("key"), _("msqid"), _("owner"), _("perms"),
			_("used-bytes"), _("messages"));
		break;
	}

	for (id = 0; id <= maxid; id++) {
		msqid = msgctl (id, MSG_STAT, &msgque);
		if (msqid < 0)
			continue;
		if (format == CREATOR)  {
			print_perms (msqid, ipcp);
			continue;
		}
		pw = getpwuid(ipcp->uid);
		switch (format) {
		case TIME:
			if (pw)
				printf ("%-8d %-10.10s", msqid, pw->pw_name);
			else
				printf ("%-8d %-10u", msqid, ipcp->uid);
			printf (" %-20.16s", msgque.msg_stime
				? ctime(&msgque.msg_stime) + 4 : _("Not set"));
			printf (" %-20.16s", msgque.msg_rtime
				? ctime(&msgque.msg_rtime) + 4 : _("Not set"));
			printf (" %-20.16s\n", msgque.msg_ctime
				? ctime(&msgque.msg_ctime) + 4 : _("Not set"));
			break;
		case PID:
			if (pw)
				printf ("%-8d %-10.10s", msqid, pw->pw_name);
			else
				printf ("%-8d %-10u", msqid, ipcp->uid);
			printf ("  %5d     %5d\n",
				msgque.msg_lspid, msgque.msg_lrpid);
			break;

		default:
			printf( "0x%08x ",ipcp->KEY );
			if (pw)
				printf ("%-10d %-10.10s", msqid, pw->pw_name);
			else
				printf ("%-10d %-10u", msqid, ipcp->uid);
			printf (" %-10o %-12ld %-12ld\n",
				ipcp->mode & 0777,
				/*
				 * glibc-2.1.3 and earlier has unsigned
				 * short. glibc-2.1.91 has variation between
				 * unsigned short, unsigned long. Austin has
				 * msgqnum_t
				 */
				(long) msgque.msg_cbytes,
				(long) msgque.msg_qnum);
			break;
		}
	}
	return;
}

void print_shm (int shmid)
{
	struct shmid_ds shmds;
	struct ipc_perm *ipcp = &shmds.shm_perm;

	if (shmctl (shmid, IPC_STAT, &shmds) == -1)
		err(EXIT_FAILURE, _("shmctl failed"));

	printf (_("\nShared memory Segment shmid=%d\n"), shmid);
	printf (_("uid=%u\tgid=%u\tcuid=%u\tcgid=%u\n"),
		ipcp->uid, ipcp->gid, ipcp->cuid, ipcp->cgid);
	printf (_("mode=%#o\taccess_perms=%#o\n"),
		ipcp->mode, ipcp->mode & 0777);
	printf (_("bytes=%lu\tlpid=%d\tcpid=%d\tnattch=%ld\n"),
		(unsigned long) shmds.shm_segsz, shmds.shm_lpid, shmds.shm_cpid,
		(long) shmds.shm_nattch);
	printf (_("att_time=%-26.24s\n"),
		shmds.shm_atime ? ctime (&shmds.shm_atime) : _("Not set"));
	printf (_("det_time=%-26.24s\n"),
		shmds.shm_dtime ? ctime (&shmds.shm_dtime) : _("Not set"));
	printf (_("change_time=%-26.24s\n"), ctime (&shmds.shm_ctime));
	printf ("\n");
	return;
}


void print_msg (int msqid)
{
	struct msqid_ds buf;
	struct ipc_perm *ipcp = &buf.msg_perm;

	if (msgctl (msqid, IPC_STAT, &buf) == -1)
		err(EXIT_FAILURE, _("msgctl failed"));

	printf (_("\nMessage Queue msqid=%d\n"), msqid);
	printf (_("uid=%u\tgid=%u\tcuid=%u\tcgid=%u\tmode=%#o\n"),
		ipcp->uid, ipcp->gid, ipcp->cuid, ipcp->cgid, ipcp->mode);
	printf (_("cbytes=%ld\tqbytes=%ld\tqnum=%ld\tlspid=%d\tlrpid=%d\n"),
		/*
		 * glibc-2.1.3 and earlier has unsigned short. glibc-2.1.91
		 * has variation between unsigned short, unsigned long.
		 * Austin has msgqnum_t (for msg_qbytes)
		 */
		(long) buf.msg_cbytes, (long) buf.msg_qbytes,
		(long) buf.msg_qnum, buf.msg_lspid, buf.msg_lrpid);
	printf (_("send_time=%-26.24s\n"),
		buf.msg_stime ? ctime (&buf.msg_stime) : _("Not set"));
	printf (_("rcv_time=%-26.24s\n"),
		buf.msg_rtime ? ctime (&buf.msg_rtime) : _("Not set"));
	printf (_("change_time=%-26.24s\n"),
		buf.msg_ctime ? ctime (&buf.msg_ctime) : _("Not set"));
	printf ("\n");
	return;
}

void print_sem (int semid)
{
	struct semid_ds semds;
	struct ipc_perm *ipcp = &semds.sem_perm;
	union semun arg;
	size_t i;

	arg.buf = &semds;
	if (semctl (semid, 0, IPC_STAT, arg) < 0)
		err(EXIT_FAILURE, _("semctl failed"));

	printf (_("\nSemaphore Array semid=%d\n"), semid);
	printf (_("uid=%u\t gid=%u\t cuid=%u\t cgid=%u\n"),
		ipcp->uid, ipcp->gid, ipcp->cuid, ipcp->cgid);
	printf (_("mode=%#o, access_perms=%#o\n"),
		ipcp->mode, ipcp->mode & 0777);
	printf (_("nsems = %ld\n"), (long) semds.sem_nsems);
	printf (_("otime = %-26.24s\n"),
		semds.sem_otime ? ctime (&semds.sem_otime) : _("Not set"));
	printf (_("ctime = %-26.24s\n"), ctime (&semds.sem_ctime));

	printf ("%-10s %-10s %-10s %-10s %-10s\n",
		_("semnum"),_("value"),_("ncount"),_("zcount"),_("pid"));
	arg.val = 0;
	for (i=0; i< semds.sem_nsems; i++) {
		int val, ncnt, zcnt, pid;
		val = semctl (semid, i, GETVAL, arg);
		ncnt = semctl (semid, i, GETNCNT, arg);
		zcnt = semctl (semid, i, GETZCNT, arg);
		pid = semctl (semid, i, GETPID, arg);
		if (val < 0 || ncnt < 0 || zcnt < 0 || pid < 0)
			err(EXIT_FAILURE, _("semctl failed"));

		printf ("%-10zd %-10d %-10d %-10d %-10d\n",
			i, val, ncnt, zcnt, pid);
	}
	printf ("\n");
	return;
}
