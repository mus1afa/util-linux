#ifndef UTIL_LINUX_IPCUTILS_H
#define UTIL_LINUX_IPCUTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <grp.h>
#include <pwd.h>

/*
 * SHM_DEST and SHM_LOCKED are defined in kernel headers, but inside
 * #ifdef __KERNEL__ ... #endif
 */
#ifndef SHM_DEST
  /* shm_mode upper byte flags */
# define SHM_DEST	01000	/* segment will be destroyed on last detach */
# define SHM_LOCKED	02000	/* segment will not be swapped */
#endif

/* For older kernels the same holds for the defines below */
#ifndef MSG_STAT
# define MSG_STAT	11
# define MSG_INFO	12
#endif

#ifndef SHM_STAT
# define SHM_STAT	13
# define SHM_INFO	14
struct shm_info {
	int used_ids;
	ulong shm_tot;		/* total allocated shm */
	ulong shm_rss;		/* total resident shm */
	ulong shm_swp;		/* total swapped shm */
	ulong swap_attempts;
	ulong swap_successes;
};
#endif

#ifndef SEM_STAT
# define SEM_STAT	18
# define SEM_INFO	19
#endif

/* Some versions of libc only define IPC_INFO when __USE_GNU is defined. */
#ifndef IPC_INFO
# define IPC_INFO	3
#endif

/*
 *  * The last arg of semctl is a union semun, but where is it defined? X/OPEN
 *   * tells us to define it ourselves, but until recently Linux include files
 *    * would also define it.
 *     */
#ifndef HAVE_UNION_SEMUN
/* according to X/OPEN we have to define it ourselves */
union semun {
	int val;
	struct semid_ds *buf;
	unsigned short int *array;
	struct seminfo *__buf;
};
#endif

/*
 * X/OPEN (Jan 1987) does not define fields key, seq in struct ipc_perm;
 *	glibc-1.09 has no support for sysv ipc.
 *	glibc 2 uses __key, __seq
 */
#if defined (__GLIBC__) && __GLIBC__ >= 2
# define KEY __key
#else
# define KEY key
#endif

struct ipc_limits {
	uint64_t	shmmni;		/* max number of segments */
	uint64_t	shmmax;		/* max segment size */
	uint64_t	shmall;		/* max total shared memory */
	uint64_t	shmmin;		/* min segment size */

	int		semmni;		/* max number of arrays */
	int		semmsl;		/* max semaphores per array */
	int		semmns;		/* max semaphores system wide */
	int		semopm;		/* max ops per semop call */
	unsigned int	semvmx;		/* semaphore max value (constant) */

	int		msgmni;		/* max queues system wide */
	size_t		msgmax;		/* max size of message */
	int		msgmnb;		/* default max size of queue */
};

extern int ipc_msg_get_limits(struct ipc_limits *lim);
extern int ipc_sem_get_limits(struct ipc_limits *lim);
extern int ipc_shm_get_limits(struct ipc_limits *lim);

#endif /* UTIL_LINUX_IPCUTILS_H */
