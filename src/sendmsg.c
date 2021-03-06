/*
    Pirate Bulletin Board System
    Copyright (C) 1990, Edward Luke, lush@Athena.EE.MsState.EDU
    Eagles Bulletin Board System
    Copyright (C) 1992, Raymond Rocker, rocker@rock.b11.ingr.com
                        Guy Vega, gtvega@seabass.st.usm.edu
                        Dominic Tynes, dbtynes@seabass.st.usm.edu
    Firebird Bulletin Board System
    Copyright (C) 1996, Hsien-Tsung Chang, Smallpig.bbs@bbs.cs.ccu.edu.tw
                        Peng Piaw Foong, ppfoong@csie.ncu.edu.tw

    Copyright (C) 1999	KCN,Zhou lin,kcn@cic.tsinghua.edu.cn
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 1, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
*/

#include "bbs.h"
#include "bbstelnet.h"
#include "edit.h"

char buf2[MAX_MSG_SIZE];
struct user_info *t_search();
int msg_blocked = 0;
extern int have_msg_unread;

static int get_msg(char *uid, char *msg, int line);
static int dowall(struct user_info *uin);
static int dowall_telnet(struct user_info *uin);
static int myfriend_wall(struct user_info *uin);
static int hisfriend_wall(struct user_info *uin);
static int sendmsgfunc(char *uid, struct user_info *uin, int userpid,
		       const char *msgstr, int mode, char *msgerr);
static void mail_msg(struct userec *user);

static int
get_msg(uid, msg, line)
char *msg, *uid;
int line;
{
	int i;

	move(line, 0);
	clrtoeol();
	prints("送音信给:%-12s    请输入音信内容，Ctrl+Q 换行:", uid);
	memset(msg, 0, sizeof (msg));
	while (1) {
		i = multi_getdata(line + 1, 0, 79, NULL, msg, MAX_MSG_SIZE, 11,
				  0);
		if (msg[0] == '\0')
			return 0;

		getdata(line + i + 1, 0,
			"确定要送出吗(Y)是的 (N)不要 (E)再编辑? [Y]: ", genbuf,
			2, DOECHO, 1);
		if (genbuf[0] == 'e' || genbuf[0] == 'E')
			continue;
		if (genbuf[0] == 'n' || genbuf[0] == 'N')
			return 0;
		return 1;
	}
}

int
canmsg(uin)
struct user_info *uin;
{
	if (!strcmp(uin->userid, "guest"))	//guest 就不收 msg 了
		return NA;
	if (isreject(uin))
		return NA;
	if ((uin->pager & ALLMSG_PAGER)
	    || HAS_PERM(PERM_SYSOP | PERM_FORCEPAGE))
		return YEA;
	if ((uin->pager & FRIENDMSG_PAGER) && hisfriend(uin))
		return YEA;
	return NA;
}

int
canmsg_offline(uid)
char *uid;
{
	if (!strcmp(uid, "guest"))
		return NA;
	if (inoverride(currentuser.userid, uid, "rejects"))
		return NA;
	if (getuser(uid) == 0)
		return NA;
	if (lookupuser.userdefine & DEF_ALLMSG)
		return YEA;
	if ((lookupuser.userdefine & DEF_FRIENDMSG)
	    && inoverride(currentuser.userid, uid, "friends"))
		return YEA;
	return NA;
}

int
s_msg()
{
	do_sendmsg(NULL, NULL, NULL, 0, 0);
	return 0;
}

int
do_sendmsg(uid, uentp, msgstr, mode, userpid)
char *uid;
struct user_info *uentp;
char msgstr[256];
int mode;
int userpid;
{
	char uident[STRLEN];
	char msgerr[256];
	struct user_info *uinptr;
	char buf[MAX_MSG_SIZE];
	int result, Gmode, upid;

	upid = userpid;
	if (mode == 0) {
		move(2, 0);
		clrtobot();
		modify_user_mode(MSG);
	}
	if (uid == NULL) {
		prints("<输入使用者代号>\n");
		move(1, 0);
		clrtoeol();
		prints("送讯息给: ");
		usercomplete(NULL, uident);
		if (uident[0] == '\0') {
			clear();
			return 0;
		}
		if (!getuser(uident)) {
			move(2, 0);
			prints("错误的帐号\n");
			pressreturn();
			return 0;
		}
		strcpy(uident, lookupuser.userid);
		uinptr = t_search(uident, NA, 0);
		if (uinptr)
			upid = uinptr->pid;
	} else {
		uinptr = uentp;
		strcpy(uident, uid);
	}
	if (uinptr == NULL) {
		uinptr = t_search(uident, NA, 0);
		if (uinptr != NULL)
			upid = uinptr->pid;
		else
			upid = 0;
	}
	if (!strcasecmp(uident, currentuser.userid))
		return 0;
	/*
	 * try to send the msg 
	 */
	result = sendmsgfunc(uident, uinptr, upid, msgstr, mode, msgerr);

	switch (result) {
	case 1:		/* success */
		return 1;
		break;
	case -1:		/* failed, reason in msgerr */
		if (mode == 2) {
			move(2, 0);
			clrtoeol();
			prints(msgerr);
			pressreturn();
			move(2, 0);
			clrtoeol();
		}
		return -1;
		break;
	case 0:		/* message presending test ok, get the message and resend */
		Gmode = get_msg(uident, buf, 1);
		if (!Gmode) {
			move(1, 0);
			clrtoeol();
			move(2, 0);
			clrtoeol();
			return 0;
		}
		mode = 2;
		break;
	default:		/* unknown reason */
		return result;
		break;
	}
	/*
	 * resend the message 
	 */
	result = sendmsgfunc(uident, uinptr, upid, buf, mode, msgerr);

	switch (result) {
	case 1:		/* success */
		return 1;
		break;
	case -1:		/* failed, reason in msgerr */
		if (mode == 2) {
			move(2, 0);
			clrtoeol();
			prints(msgerr);
			pressreturn();
			move(2, 0);
			clrtoeol();
		}
		return -1;
		break;
	default:		/* unknown reason */
		return result;
		break;
	}
	return 1;
}

static int
dowall(uin)
struct user_info *uin;
{
	if (!uin->active || !uin->pid)
		return -1;
	move(1, 0);
	clrtoeol();
	prints("[1;32m正对 %s 广播.... Ctrl-D 停止对此位 User 广播。[m",
	       uin->userid);
	refresh();
	do_sendmsg(uin->userid, uin, buf2, 0, uin->pid);
	return 0;
}

static int
dowall_telnet(uin)
struct user_info *uin;
{
	if (!uin->active || !uin->pid || uin->pid ==1)
		return -1;
	move(1, 0);
	clrtoeol();
	prints("[1;32m正对 %s 广播.... [m", uin->userid);
	refresh();
	do_sendmsg(uin->userid, uin, buf2, 0, uin->pid);
	return 0;
}

static int
myfriend_wall(uin)
struct user_info *uin;
{
	if ((uin->pid - uinfo.pid == 0) || !uin->active || !uin->pid
	    || isreject(uin))
		return -1;
	if (myfriend(uin->uid)) {
		move(1, 0);
		clrtoeol();
		prints("[1;32m正在送讯息给 %s...  [m", uin->userid);
		refresh();
		do_sendmsg(uin->userid, uin, buf2, 3, uin->pid);
	}
	return 0;
}

static int
hisfriend_wall(uin)
struct user_info *uin;
{
	if ((uin->pid - uinfo.pid == 0) || !uin->active || !uin->pid
	    || isreject(uin))
		return -1;
	if (hisfriend(uin)) {
		move(1, 0);
		clrtoeol();
		prints("[1;32m正在送讯息给 %s...  [m", uin->userid);
		refresh();
		do_sendmsg(uin->userid, uin, buf2, 3, uin->pid);
	}
	return 0;
}

int
wall()
{
	if (!HAS_PERM(PERM_SYSOP))
		return 0;
	modify_user_mode(MSG);
	move(2, 0);
	clrtobot();
	if (!get_msg("所有使用者", buf2, 1)) {
		return 0;
	}
	if (apply_ulist(dowall) == 0) {
		move(2, 0);
		prints("线上空无一人\n");
		pressanykey();
	}
	prints("\n已经广播完毕...\n");
	pressanykey();
	return 1;
}

int
wall_telnet()
{
	if (!HAS_PERM(PERM_SYSOP))
		return 0;
	modify_user_mode(MSG);
	move(2, 0);
	clrtobot();
	if (!get_msg("telnet用户", buf2, 1)) {
		return 0;
	}
	if (apply_ulist(dowall_telnet) == 0) {
		move(2, 0);
		prints("线上空无一人\n");
		pressanykey();
	}
	prints("\n已经广播完毕...\n");
	pressanykey();
	return 1;
}

int
friend_wall()
{
	char buf[3] = "";

	if (uinfo.invisible) {
		move(2, 0);
		prints("抱歉, 此功能在隐身状态下不能执行...\n");
		pressreturn();
		return 0;
	}
	modify_user_mode(MSG);
	move(2, 0);
	clrtobot();
	getdata(4, 0,
		"送讯息给 [[1;32m1[m] 我的好朋友，[[32;1m2[m] 与我为友者: ",
		buf, 2, DOECHO, YEA);
	switch (buf[0]) {
	case '1':
		if (!get_msg("我的好朋友", buf2, 1))
			return 0;
		if (apply_ulist(myfriend_wall) == -1) {
			move(2, 0);
			prints("线上空无一人\n");
			pressanykey();
		}
		break;
	case '2':
		if (!get_msg("与我为友者", buf2, 1))
			return 0;
		if (apply_ulist(hisfriend_wall) == -1) {
			move(2, 0);
			prints("线上空无一人\n");
			pressanykey();
		}
		break;
	default:
		return 0;
	}
	move(6, 0);
	limit_cpu();
	prints("讯息传送完毕...");
	pressanykey();
	return 1;
}

void
r_msg2()
{
	char buf[MAX_MSG_SIZE];
	char savebuffer[25][LINELEN * 3];
	char outmsg[MAX_MSG_SIZE * 2];
	int line, tmpansi;
	int i, y, x, ch, count;
	int MsgNum;
	int savemode;
	int totalmsg;
	struct msghead head;

	block_msg();
	savemode = uinfo.mode;
	modify_user_mode(MSG);
	getyx(&y, &x);
	line = y;
	totalmsg = 0;
	count = get_msgcount(1, currentuser.userid);
	if (count == 0)
		return;
	tmpansi = showansi;
	showansi = 1;
	for (i = 0; i <= 23; i++)
		saveline(i, 0, savebuffer[i]);
	MsgNum = count - 1;
	RMSG = YEA;
	while (1) {
		for (i = 0; i <= 23; i++)
			saveline(i, 1, savebuffer[i]);
		MsgNum = (MsgNum % count);
		load_msghead(1, currentuser.userid, &head, MsgNum);
		load_msgtext(currentuser.userid, &head, buf);
		line = translate_msg(buf, &head, outmsg, inBBSNET);
		for (i = 0; i <= line; i++) {
			move(i, 0);
			clrtoeol();
		}
		move(0, 0);
		prints("%s", outmsg);
		{
			struct user_info *uin = NULL;
			int send_pid, msgperm_test, online_test;
			int userpid;
			int invisible = 0;
			char usid[STRLEN] = "(NULL)";

			send_pid = head.frompid;
			strcpy(usid, head.id);
			if (head.mode != 0) {
				uin = t_search(usid, send_pid, 0);
				if (uin == NULL) {
					online_test = 0;
					msgperm_test = canmsg_offline(usid);
					userpid = 0;
				} else {
					online_test = 1;
					msgperm_test = canmsg(uin);
					userpid = uin->pid;
					invisible = (uin->invisible
						     && !HAS_PERM(PERM_SEECLOAK
								  |
								  PERM_SYSOP));
				}
			} else {
				online_test = 0;
				msgperm_test = 0;
				userpid = 0;
			}

			prints
			    ("第 %d 条消息，共 %d 条消息，回复 %s(%s)，换行按 Ctrl+Q\n",
			     MsgNum + 1, count, head.id,
			     (online_test
			      && !invisible) ? "在线" : "\x1b[1;32m离线\x1b[m");
			if (msgperm_test) {
				ch = multi_getdata(line + 1, 0, 79, NULL,
						   buf, MAX_MSG_SIZE, 11, 1);
				if (-ch == Ctrl('Z') || -ch == KEY_UP) {
					MsgNum--;
					if (MsgNum < 0)
						MsgNum = count - 1;
					continue;
				} else if (-ch == Ctrl('A') || -ch == KEY_DOWN) {
					MsgNum++;
					continue;
				}
				if (buf[0] != '\0') {
					if (do_sendmsg
					    (usid, uin, buf, 2, userpid) == 1) {
						prints("\n");
						clrtoeol();
						prints
						    ("[1;32m帮你送出讯息给 %s 了![m",
						     usid);
						refresh();
						sleep(1);
					}
				} else {
					prints("\n");
					clrtoeol();
					prints("[1;33m空讯息, 所以不送出.[m");
					refresh();
					sleep(1);
				}
			} else {
				clrtoeol();
				prints("\n");
				clrtoeol();
				prints
				    ("[1;32m无法发讯息给 %s! 请按上:[^Z ↑] 或下:[^A ↓] 或按其他键离开 [m",
				     usid);
				ch = igetkey();
				if (ch == Ctrl('Z') || ch == KEY_UP) {
					MsgNum--;
					if (MsgNum < 0)
						MsgNum = count - 1;
					continue;
				}
				if (ch == Ctrl('A') || ch == KEY_DOWN) {
					MsgNum++;
					continue;
				}
			}
		}
		break;
	}

	showansi = tmpansi;
	for (i = 0; i <= 23; i++)
		saveline(i, 1, savebuffer[i]);
	move(y, x);
	refresh();
	modify_user_mode(savemode);
	RMSG = NA;
	unblock_msg();
	return;
}

void
r_msg()
{
	char buf[MAX_MSG_SIZE];
	char outmsg[MAX_MSG_SIZE * 2];
	char savebuffer[25][LINELEN * 3];
	int line, tmpansi, i;
	int y, x, premsg = NA, newmsg;
	int count;
	int savemode;
	char ch;
	struct msghead head;
	if (msg_blocked) {
		have_msg_unread = 1;
		return;
	}
	signal(SIGUSR2, SIG_IGN);
	savemode = uinfo.mode;
	modify_user_mode(MSG);
	getyx(&y, &x);
	tmpansi = showansi;
	showansi = 1;
	if (DEFINE(DEF_MSGGETKEY)) {
		for (i = 0; i <= 23; i++)
			saveline(i, 0, savebuffer[i]);
		premsg = RMSG;
	}
	newmsg = get_unreadcount(currentuser.userid);
	while (newmsg) {
		if (DEFINE(DEF_SOUNDMSG)) {
			bell();
		}
		count = get_unreadmsg(currentuser.userid);
		load_msghead(1, currentuser.userid, &head, count);
		load_msgtext(currentuser.userid, &head, buf);
		line = translate_msg(buf, &head, outmsg, inBBSNET);
		for (i = 0; i < line; i++) {
			move(i, 0);
			clrtoeol();
		}
		move(0, 0);
		prints("%s", outmsg);
		clrtoeol();
		prints("第 %d 条消息，共 %d 条消息 按r回复", count + 1,
		       count + 1);
		getyx(&line, &i);
		if (DEFINE(DEF_MSGGETKEY)) {
			RMSG = YEA;
			ch = 0;
			while (ch != '\r' && ch != '\n') {
				ch = igetkey();
				if (ch == '\r' || ch == '\n')
					break;
				else if (ch == Ctrl('R') || ch == 'R'
					 || ch == 'r' || ch == Ctrl('Z')) {
					int send_pid, msgperm_test, online_test;
					int userpid;
					int invisible = 0;
					char usid[STRLEN] = "(NULL)";
					struct user_info *uin = NULL;

					send_pid = head.frompid;
					strcpy(usid, head.id);
					if (head.mode != 0) {
						uin =
						    t_search(usid, send_pid, 0);
						if (uin == NULL) {
							online_test = 0;
							msgperm_test =
							    canmsg_offline
							    (usid);
							userpid = 0;
						} else {
							online_test = 1;
							msgperm_test =
							    canmsg(uin);
							userpid = uin->pid;
							invisible =
							    (uin->invisible
							     &&
							     !HAS_PERM
							     (PERM_SEECLOAK |
							      PERM_SYSOP));
						}
					} else {
						online_test = 0;
						msgperm_test = 0;
						userpid = 0;
					}

					send_pid = head.frompid;
					strcpy(usid, head.id);
					if (msgperm_test) {
						clrtoeol();
						prints
						    ("，回讯息给 %s (%s)，Ctrl+Q换行: ",
						     usid, (online_test
						      && !invisible) ? "在线" :
						     "\x1b[1;32m离线\x1b[m");
						move(line + 1, 0);
						clrtoeol();
						multi_getdata(line + 1, 0, 79,
							      NULL, buf,
							      MAX_MSG_SIZE, 11,
							      1);

						if (buf[0] != '\0'
						    && buf[0] != Ctrl('Z')
						    && buf[0] != Ctrl('A')) {
							if (do_sendmsg
							    (usid, uin, buf, 2,
							     userpid) == 1) {
								prints("\n");
								clrtoeol();
								prints
								    ("[1;32m帮你送出讯息给 %s 了![m",
								     usid);
								refresh();
								sleep(1);
							}
						} else {
							prints("\n");
							clrtoeol();
							prints
							    ("[1;33m空讯息, 所以不送出.[m");
							refresh();
							sleep(1);
						}
					} else {
						prints("\n");
						clrtoeol();
						prints
						    ("[1;32m找不到发讯息的 %s.[m",
						     usid);
						refresh();
						sleep(1);
					}
					break;
				}
			}
		}
		newmsg = get_unreadcount(currentuser.userid);
		if (DEFINE(DEF_MSGGETKEY)) {
			for (i = 0; i <= 23; i++)
				saveline(i, 1, savebuffer[i]);
		}
	}

	if (DEFINE(DEF_MSGGETKEY)) {
		RMSG = premsg;
	}
	showansi = tmpansi;
	move(y, x);
	refresh();
	have_msg_unread = 0;
	modify_user_mode(savemode);
	signal(SIGUSR2, (void *) r_msg);
	return;
}

void
block_msg()
{
	msg_blocked = 1;
}

void
unblock_msg()
{
	msg_blocked = 0;
	r_msg();
}

int
friend_login_wall(pageinfo)
struct user_info *pageinfo;
{
	char msg[STRLEN];
	int x, y;

	if (!pageinfo->active || !pageinfo->pid || isreject(pageinfo))
		return 0;
	if (hisfriend(pageinfo)) {
		if (getuser(pageinfo->userid) <= 0)
			return 0;
		if (!(lookupuser.userdefine & DEF_LOGINFROM))
			return 0;
		if (!strcmp(pageinfo->userid, currentuser.userid))
			return 0;
		getyx(&y, &x);
		if (y > 22) {
			pressanykey();
			move(7, 0);
			clrtobot();
		}
		prints("送出好友上站通知给 %s\n", pageinfo->userid);
		sprintf(msg, "你的好朋友 %s 已经上站罗！", currentuser.userid);
		do_sendmsg(pageinfo->userid, pageinfo, msg, 2, pageinfo->pid);
	}
	return 0;
}

static int
sendmsgfunc(char *uid, struct user_info *uin, int userpid, const char *msgstr,
	    int mode, char *msgerr)
{
	struct msghead head, head2;
	int offline_msg = 0;
	int topid;

	*msgerr = 0;
	if (msgstr == NULL) {
		return 0;
	}
	if (0 == uid[0])
		return -1;
	if (mode != 0) {
		if (get_unreadcount(uid) > MAXMESSAGE) {
			strcpy(msgerr,
			       "对方尚有一些讯息未处理，请稍候再发或给他(她)写信...");
			return -1;
		}
	}
	if (uin == NULL) {
		offline_msg = 1;
	} else {
		if (strcmp(uid, uin->userid)) {
			offline_msg = 1;
		} else if (userpid) {
			if (userpid != uin->pid) {
				offline_msg = 1;
			}
		} else if (!uin->active || uin->pid <= 0
			   || (uin->pid != 1 && kill(uin->pid, 0) == -1)) {
			offline_msg = 1;
		} else if (uin->mode == IRCCHAT || uin->mode == BBSNET
			   || uin->mode == HYTELNET || uin->mode == GAME
			   || uin->mode == PAGE || uin->mode == LOCKSCREEN) {
			offline_msg = 1;
		}
	}
	if (offline_msg) {
		if (!canmsg_offline(uid)) {
			strcpy(msgerr, "无法发送消息");
			return -1;
		}
		topid = 0;
	} else {
		if (!canmsg(uin)) {
			strcpy(msgerr, "无法发送消息");
			return -1;
		}
		topid = uin->pid;
	}
	head.time = time(0);
	head.sent = 0;
	head.mode = mode;
	strncpy(head.id, currentuser.userid, IDLEN + 2);
	head.frompid = uinfo.pid;
	head.topid = topid;
	memcpy(&head2, &head, sizeof (struct msghead));
	head2.sent = 1;
	strncpy(head2.id, uid, IDLEN + 2);

	if (save_msgtext(uid, &head, msgstr) < 0)
		return -2;
	if (strcmp(currentuser.userid, uid) && mode != 3) {
		if (save_msgtext(currentuser.userid, &head2, msgstr) < 0)
			return -2;
	}
	if (!offline_msg) {
		if (uin->pid != 1)
			kill(uin->pid, SIGUSR2);
		else
			(uin->unreadmsg)++;
	}
	return 1;
}

int
show_allmsgs()
{
	char buf[MAX_MSG_SIZE], showmsg[MAX_MSG_SIZE * 2], chk[STRLEN];
	int oldmode, count, i, j, page, ch, y, all = 0, reload = 0;
	struct msghead head;

	if (!HAS_PERM(PERM_PAGE))
		return -1;
	oldmode = uinfo.mode;
	modify_user_mode(LOOKMSGS);

	page = 0;
	count = get_msgcount(0, currentuser.userid);
	while (1) {
		if (reload) {
			reload = 0;
			page = 0;
			count = get_msgcount(all ? 2 : 0, currentuser.userid);
		}
		clear();
		if (count == 0) {
			move(5, 30);
			prints("\x1b[m没有任何的讯息存在！！");
			i = 0;
		} else {
			y = 0;
			i = page;
			load_msghead(all ? 2 : 0, currentuser.userid, &head, i);
			load_msgtext(currentuser.userid, &head, buf);
			j = translate_msg(buf, &head, showmsg, inBBSNET);
			while (y + j <= t_lines - 1) {
				y += j;
				i++;
				prints("%s\x1b[m", showmsg);
				clrtoeol();
				if (i >= count)
					break;
				load_msghead(all ? 2 : 0,
					     currentuser.userid, &head, i);
				load_msgtext(currentuser.userid, &head, buf);
				j = translate_msg(buf, &head, showmsg, inBBSNET);
			}
		}
		move(t_lines - 1, 0);
		if (!all)
			prints
			    ("\x1b[1;44;32m保留<\x1b[37mr\x1b[32m> 清除<\x1b[37mc\x1b[32m> 寄回信箱<\x1b[37mm\x1b[32m> 发讯人<\x1b[37mi\x1b[32m> 讯息内容<\x1b[37ms\x1b[32m> 头<\x1b[37mh\x1b[32m> 尾<\x1b[37me\x1b[32m>        剩余:%4d\x1b[m",
			     count - i);
		else
			prints
			    ("\x1b[1;44;32m保留<\x1b[37mr\x1b[32m> 清除<\x1b[37mc\x1b[32m> 寄回信箱<\x1b[37mm\x1b[32m> 发讯人<\x1b[37mi\x1b[32m> 讯息内容<\x1b[37ms\x1b[32m> 全部<\x1b[37ma\x1b[32m> 头<\x1b[37mh\x1b[32m> 尾<\x1b[37me\x1b[32m>      %4d\x1b[m",
			     count - i);
	      reenter:
		ch = igetkey();
		switch (ch) {
		case 'r':
		case 'R':
		case 'q':
		case 'Q':
		case KEY_LEFT:
		case '\r':
		case '\n':
			goto outofhere;
		case KEY_UP:
			if (page > 0)
				page--;
			break;
		case KEY_DOWN:
			if (page < count - 1)
				page++;
			break;
		case KEY_PGDN:
		case ' ':
		case KEY_RIGHT:
			if (page < count - 11)
				page += 10;
			else
				page = count - 1;
			break;
		case KEY_PGUP:
			if (page > 10)
				page -= 10;
			else
				page = 0;
			break;
		case KEY_HOME:
		case Ctrl('A'):
		case 'H':
		case 'h':
			page = 0;
			break;
		case KEY_END:
		case Ctrl('E'):
		case 'E':
		case 'e':
			page = count - 1;
			break;
		case 'i':
		case 'I':
		case 's':
		case 'S':
			reload = 1;
			count = get_msgcount(0, currentuser.userid);
			if (count == 0)
				break;
			move(t_lines - 1, 0);
			clrtoeol();
			getdata(t_lines - 1, 0, "请输入关键字:", chk, 50, 1, 1);
			if (chk[0]) {
				int fd, fd2;
				char fname[STRLEN], fname2[STRLEN];
				struct msghead head;
				int i;
				sethomefile(fname, currentuser.userid,
					    "msgindex");
				sethomefile(fname2, currentuser.userid,
					    "msgindex3");
				fd = open(fname, O_RDONLY, 0644);
				fd2 = open(fname2, O_WRONLY | O_CREAT, 0644);
				write(fd2, &i, 4);
				lseek(fd, 4, SEEK_SET);
				for (i = 0; i < count; i++) {
					read(fd, &head,
					     sizeof (struct msghead));
					if (toupper(ch) == 'S')
						load_msgtext
						    (currentuser.userid,
						     &head, buf);
					if ((toupper(ch) == 'I'
					     && !strncasecmp(chk,
							     head.id, IDLEN))
					    || (toupper(ch) == 'S'
						&& strcasestr(buf,
							      chk) != NULL))
						write(fd2, &head,
						      sizeof (struct msghead));
				}
				close(fd2);
				close(fd);
				all = 1;
			}

			break;
		case 'c':
		case 'C':
			clear_msg(currentuser.userid);
			goto outofhere;
		case 'a':
		case 'A':
			if (all) {
				sethomefile(buf, currentuser.userid,
					    "msgindex3");
				unlink(buf);
				all = 0;
				reload = 1;
			}
			break;
		case 'm':
		case 'M':
			if (count != 0)
				mail_msg(&currentuser);
			goto outofhere;
		default:
			goto reenter;
		}
	}
      outofhere:

	if (all) {
		sethomefile(buf, currentuser.userid, "msgindex3");
		unlink(buf);
	}
	clear();
	modify_user_mode(oldmode);
	return 0;
}

static void
mail_msg(struct userec *user)
{
	char fname[30];
	char buf[MAX_MSG_SIZE], showmsg[MAX_MSG_SIZE * 2];
	int i;
	struct msghead head;
	time_t now;
	char title[STRLEN];
	FILE *fn;
	int count;

	sprintf(fname, "tmp/%s.msg", user->userid);
	fn = fopen(fname, "w");
	count = get_msgcount(0, user->userid);
	for (i = 0; i < count; i++) {
		load_msghead(0, user->userid, &head, i);
		load_msgtext(user->userid, &head, buf);
		translate_msg(buf, &head, showmsg, inBBSNET);
		fprintf(fn, "%s", showmsg);
	}
	fclose(fn);

	now = time(0);
	sprintf(title, "[%12.12s] 所有讯息备份", ctime(&now) + 4);
	mail_file(fname, user->userid, title);
	unlink(fname);
	clear_msg(user->userid);
}
