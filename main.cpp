/*
 * Copyright (C) 2015 Shilin Victor chrono.monochrome@gmail.com
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h> 
#include <stdio.h>
#include <unistd.h> // read, write, close
#include <fcntl.h> // open
#include <sys/stat.h>
#include <ctype.h> // isdigit, isspace

#include "block.h"
#include "misc.h"
#include "log.h"

#define INPUT "/ramdisk/fstab.tmp"
#define OUTPUT "/ramdisk/fstab"

#define F2FS_MAGIC 0xF2F52010

#define UID_SYSTEM 1000
#define GID_SYSTEM 1000

char *fstab_p[10];
char fstab[10][512]; // assume fstab contains up to 10 lines up to 512 bytes each

struct partition_t {
	const char * name;
	unsigned int volume_number;
	unsigned int partition_number;
	const char * ext4_flags;
	const char * f2fs_flags;
	bool can_be_f2fs;
	bool use_in_parsing;
};


struct partition_t partition_table[] {
	{"/system", 0, 3, "ext4 ro,noatime,errors=panic wait", "f2fs ro wait", 1, 1},
	{"/cache", 0, 4, "ext4 noatime,nosuid,nodev,errors=panic wait,check", 
		"f2fs rw,discard,nosuid,nodev,noatime,nodiratime,flush_merge,\
background_gc=off,inline_xattr,active_logs=2 wait", 1, 1},
	{"/modemfs", 0, 2, "ext4 noatime,nosuid,nodev,errors=panic wait,check",
		"ext4 noatime,nosuid,nodev,errors=panic wait,check", 0, 1}, 
	{" /efs", 0, 7, "ext4 noatime,nosuid,nodev,errors=panic wait,check",
		"ext4 noatime,nosuid,nodev,errors=panic wait,check", 0, 1},
	{"/data", 0, 5, "ext4 noatime,nosuid,nodev,discard,noauto_da_alloc,errors=panic \
wait,check,encryptable=/efs/metadata", 
		"f2fs rw,discard,nosuid,nodev,noatime,nodiratime,flush_merge,background_gc=off,\
inline_xattr,active_logs=2 wait,nonremovable,encryptable=/efs/metadata", 1, 1},
	{"/preload", 0, 9, "ext4 ro,noatime,errors=panic wait", "f2fs ro wait", 1, 1},
	{"/boot", 0, 15, "emmc defaults recoveryonly", "emmc defaults recoveryonly", 0, 1},
};

static int is_f2fs(int volume_number, int partition_number) {
	int fd;
	char device[50];
	unsigned magic;
	
	sprintf(device, "/dev/block/mmcblk%dp%d", volume_number, partition_number);
	fd = open(device, O_RDONLY);

	if (fd != -1) {
		lseek(fd, 1024, SEEK_SET);
		read(fd, &magic, sizeof(unsigned));
		close(fd);

		if (magic == F2FS_MAGIC) return 1;
        	return 0;
	}

	return -1;
}

static int __do_write_line(int fd, int idx, int volume_number, int partition_number) {
	char buf[512];
	char partition_flags[376];
	
	// by default assume that parition is ext4
	sprintf(partition_flags, "%s",  partition_table[idx].ext4_flags);
	if (partition_table[idx].can_be_f2fs) {
		if (is_f2fs(volume_number, partition_number) == 1) {
			memset(partition_flags, 0, strlen(partition_flags));
			sprintf(partition_flags, "%s",  partition_table[idx].f2fs_flags);
		}
	}
			
	sprintf(buf, "/dev/block/mmcblk%dp%d %s %s\n", volume_number, partition_number, partition_table[idx].name, partition_flags);	
	write(fd, buf, strlen(buf));

	return 0;
}

static int do_write_line(int fd, int idx) {
	return __do_write_line(fd, idx, partition_table[idx].volume_number, 
					partition_table[idx].partition_number);
}

static int parse_and_write_partition_line(int fd, int idx) {
	int i, j, x = 0, tmp, new_volume_number, new_partition_number;

	char *pstr;
	
	if (!fd || !fstab_p) {
		pr_err("%s: !fd || !fstab_p\n", __func__);
		return 1;
	}

	for (i = 0; i < ARRAY_SIZE(partition_table) - 1; i++) {
		//istr = strstr(estr, partition_table[i].name);

		if (strstr(fstab_p[i], partition_table[idx].name)) {
			pstr = strstr(fstab_p[i], BLOCK_PATTERN);
			tmp = pstr - fstab_p[i] + strlen(BLOCK_PATTERN);
			
			if (!(isdigit(pstr[tmp]) && isdigit(pstr[tmp + 2]))) {
				pr_err("%s: not correct partition/volume number: %s\n", __func__, fstab_p[i]);
				return 1;
			}
			new_volume_number = atoi(pstr[tmp]);
			new_partition_number = atoi(pstr[tmp + 2]);

			if (isdigit(pstr[tmp + 3]))
				new_partition_number = new_partition_number * 10 + atoi(pstr[tmp + 3]);
			
		
			for (j = 0; j < ARRAY_SIZE(partition_table) - 1; j++) {
				if (partition_table[j].partition_number == new_partition_number) {
					/* avoid having two or more entries in fstab with same partition number */
					partition_table[j].use_in_parsing = false;
				}
			}
			
			__do_write_line(fd, i, new_volume_number, new_partition_number);
			return 0;
		}
	}
	
	pr_err("%s: partition %s not found\n", __func__, partition_table[i].name);

	return 1;
}

static int parse_and_write_vold_lines(int fd) {
	char buf[512];
	char *istr;
	int i, idx, new_sdcard_number, ret = -2;
	
	for (i = 0; i < ARRAY_SIZE(partition_table); i++) {
		if (strstr(fstab_p[i], SDCARD0) || strstr(fstab_p[i], SDCARD1)) {
			ret += 1;
			istr = strstr(fstab_p[i], VOLD_PATTERN);
			idx = istr - fstab_p[i] + strlen(VOLD_PATTERN);
			new_sdcard_number = atoi(fstab_p[i][idx]);

			if (strstr(fstab_p[i], SDCARD0))
				sprintf(buf, "%s auto auto defaults voldmanaged=sdcard%d:8,nonremovable,noemulatedsd\n", SDCARD0, new_sdcard_number);
			else if (strstr(fstab_p[i], SDCARD1))
				sprintf(buf, "%s auto auto defaults voldmanaged=sdcard%d:auto\n", SDCARD1, new_sdcard_number);
		
			write(fd, buf, strlen(buf));
		} 
	}

	if (ret < 0) {
		pr_err("%s: some vold managed devices weren't found\n");
		return 1;
	}

	return 0;
}

int main (int argc, char *argv[]) { 
	struct stat st; 
	FILE *mf;
	char *pstr;
	char str[512];
	int i, fd1, read_lines = 0;
	
	bool should_create_fstab = false;

	int ret = 0;

	if (!stat(INPUT, &st)) { // first remove ./fstab.tmp
		ret = remove(INPUT);
		//printf("remove(INPUT): %d\n", ret);
		if (ret)
			pr_err("%s: unable to remove %s\n", __func__, INPUT);
	}
	
	if (!stat(OUTPUT, &st)) // if ./fstab is exists
	{	
		// rename ./fstab -> ./fstab.tmp
		ret = rename(OUTPUT, INPUT);
		//printf("rename(OUTPUT, INPUT): %d\n", ret);
		if (ret) {
			pr_err("%s: unable to rename %s -> %s, trying to remove %s\n",
				 __func__, OUTPUT, INPUT, OUTPUT);
			ret = remove(OUTPUT);
			//printf("remove(OUTPUT): %d\n", ret);
			if (ret) {
				pr_err("%s: fatal: unable to remove %s\n", __func__, OUTPUT);
				return -1;
			}
		}
	}

	if (!stat(OUTPUT, &st) && stat(INPUT, &st)) {
		rename(OUTPUT, INPUT);
	}

	mf = fopen(INPUT, "r");
	fd1 = open(OUTPUT, O_WRONLY | O_CREAT | O_TRUNC, 0640);
	
	if (!fd1) {
		pr_err("%s: fatal: %s not opened!\n", __func__, OUTPUT);
		return 0;
	}		

	if (!mf) {
		pr_err("%s: %s not opened!\n", __func__, INPUT);
   		should_create_fstab = true;
		// fstab doesn't exist, fallback to defaults
		goto FALLBACK;
	}

	read_lines = 0;
	
	// if old fstab is exist, try gathering data about partition numbers from it
	if (mf && !should_create_fstab) {
   		
		while ( (fstab_p[read_lines] = fgets(fstab[read_lines], sizeof(fstab[read_lines]), mf)) &&
				 read_lines < ARRAY_SIZE(fstab)) {
			read_lines++;
		}
		fclose(mf);

		for (i = 0; i < ARRAY_SIZE(partition_table) - 2; i++) {
			// try to parse an existing fstab first
			if (parse_and_write_partition_line(fd1, i)) {
				// fallback to default
				if (partition_table[i].use_in_parsing)
					do_write_line(fd1, i);
			}
		}

		parse_and_write_vold_lines(fd1);
		i = ARRAY_SIZE(partition_table) - 1;
		do_write_line(fd1, i);		
	} 

FALLBACK:
	if (should_create_fstab) {
		// remove fstab first
		close(fd1);
		remove(OUTPUT);
		fd1 = open(OUTPUT, O_WRONLY | O_CREAT | O_TRUNC, 0640);
		for (i = 0; i < ARRAY_SIZE(partition_table) - 1; i++)
			do_write_line(fd1, i);

		if (str)
			memset(str, 0, strlen(str));
		
		sprintf(str, "%s auto auto defaults voldmanaged=sdcard0:8,nonremovable,noemulatedsd\n", SDCARD0);
		write(fd1, str, strlen(str));
		sprintf(str, "%s auto auto defaults voldmanaged=sdcard1:auto\n", SDCARD1);
		write(fd1, str, strlen(str));
		
		do_write_line(fd1, ARRAY_SIZE(partition_table) - 1);
	}

	close(fd1);
	ret = chown(OUTPUT, UID_SYSTEM, GID_SYSTEM);	
	//pr_err("chown(OUTPUT): %d\n", ret);

	return 0;
} 
