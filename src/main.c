#include <stdio.h>
#include <net/if.h>
#include <linux/netlink.h>
#include <linux/genetlink.h>
#include <linux/nl80211.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <linux/prctl.h>
#include <sys/prctl.h>
#include <stdatomic.h>
#include "hds.h"

#define IEEE80211_WLAN_EID_SSID 0

static _Atomic char rfl = 1;
static int nlf_id = -1;

static void sig_hand(int sig)
{
	atomic_store_explicit(&rfl, 0, memory_order_release);
}

static __u32 wlifi(void)
{	
	int s = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_GENERIC);
	if(s < 0)
	{
		return -1;
	}

	struct sockaddr_nl local = { .nl_family = AF_NETLINK };
	if(bind(s, (struct sockaddr*)&local, sizeof(local)))
	{
		close(s);
		return -1;
	}

	struct 
	{
		struct nlmsghdr nh;
		struct genlmsghdr gnh;
		__u32 ifind;

	} sbuf = 
	{
		.nh = 
		{ 
			.nlmsg_len = sizeof(sbuf),
			.nlmsg_type = (__u16)nlf_id,
			.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP
		},
		.gnh = 
		{
			.cmd = NL80211_CMD_GET_INTERFACE,
			.version = 1
		}
	};

	struct sockaddr_nl ker = { .nl_family = AF_NETLINK };
	struct iovec iov = 
	{
		.iov_base = &sbuf,
		.iov_len = sizeof(sbuf)
	};
	struct msghdr msg = 
	{
		.msg_name = &ker,
		.msg_namelen = sizeof(ker),
		.msg_iov = &iov,
		.msg_iovlen = 1
	};
	
	if(sendmsg(s, &msg, 0) == -1)
	{
		close(s);
		return -1;
	}
	
	char buf[8192] = { 0 };
	iov.iov_base = buf;
	iov.iov_len = 8192;
	ssize_t rs = 0;
	__u32 ifi = 0;
	__u32 iftype = 0;
	struct nlmsghdr* nh = NULL;
	while((rs = recvmsg(s, &msg, 0)) != -1)
	{
		if(((struct sockaddr_nl*)msg.msg_name)->nl_pid && ((struct sockaddr_nl*)msg.msg_name)->nl_family != AF_NETLINK)
		{
			break;
		}	
		for(nh = (struct nlmsghdr*)buf; NLMSG_OK(nh, rs) && (!ifi || !iftype); nh = NLMSG_NEXT(nh,rs))
		{
			if(nh->nlmsg_type == NLMSG_DONE)
			{
				break;
			}
			char* nat = (char*)nh + sizeof(struct nlmsghdr) + sizeof(struct genlmsghdr);
			ifi = iftype = 0;
			while((nat < (char*)nh + nh->nlmsg_len))
			{
				if(((struct nlattr*)nat)->nla_type == NL80211_ATTR_IFTYPE)
				{
					if(*(__u32*)(nat + sizeof(struct nlattr)) == NL80211_IFTYPE_STATION)
					{
						iftype = NL80211_IFTYPE_STATION;
					}
				}
				else if(((struct nlattr*)nat)->nla_type == NL80211_ATTR_IFINDEX)
				{
					ifi = *(__u32*)(nat + sizeof(struct nlattr));
				}

				if(iftype && ifi)
				{
					break;
				}
				nat += NLA_ALIGN(((struct nlattr*)nat)->nla_len);
			}
		}
		memset(buf, 0, rs);
		if(nh->nlmsg_type == NLMSG_DONE)
		{
			break;
		}
	}

	close(s);
	return ifi;
}

int main(void)
{
	prctl(PR_SET_NO_NEW_PRIVS, 1L, 0L, 0L, 0L);
	
	int gns = socket(AF_NETLINK, SOCK_RAW | SOCK_NONBLOCK | SOCK_CLOEXEC, NETLINK_GENERIC);
	if(gns < 0)
	{
		return -1;
	}

	struct sockaddr_nl local = { .nl_family = AF_NETLINK };
	if(bind(gns, (struct sockaddr*)&local, sizeof(local)))
	{
		close(gns);
		return -1;
	}

	struct {
		struct nlmsghdr nh;
		struct genlmsghdr gnh;
		struct nlattr nat;
		char data[sizeof(NL80211_GENL_NAME)];
	} f_d_buf = 
	{
		.nh = 
		{ 
			.nlmsg_len = sizeof(f_d_buf),
			.nlmsg_type = GENL_ID_CTRL,
			.nlmsg_flags = NLM_F_REQUEST
		},
		.gnh = 
		{
			.cmd = CTRL_CMD_GETFAMILY,
			.version = 1
		},
		.nat = 
		{
			.nla_len = sizeof(struct nlattr) + sizeof(NL80211_GENL_NAME),
			.nla_type = CTRL_ATTR_FAMILY_NAME
		}
	};

	memcpy(f_d_buf.data, NL80211_GENL_NAME, sizeof(NL80211_GENL_NAME));

	struct sockaddr_nl ker = { .nl_family = AF_NETLINK };
	struct iovec iov = 
	{
		.iov_base = &f_d_buf,
		.iov_len = sizeof(f_d_buf)
	};
	struct msghdr msg = 
	{
		.msg_name = &ker,
		.msg_namelen = sizeof(ker),
		.msg_iov = &iov,
		.msg_iovlen = 1
	};
	
	if(sendmsg(gns, &msg, 0) == -1)
	{
		close(gns);
		return -1;
	}
	
	long ml_gid = -1;
	char buf[8192] = { 0 };
	iov.iov_base = buf;
	iov.iov_len = 8192;
	ssize_t rs = 0;
	while((rs = recvmsg(gns, &msg, 0)) != -1)
	{
		if(((struct sockaddr_nl*)msg.msg_name)->nl_pid && ((struct sockaddr_nl*)msg.msg_name)->nl_family != AF_NETLINK)
		{
			close(gns);
			return -1;
		}	

		for(struct nlmsghdr* nh = (struct nlmsghdr*)buf; NLMSG_OK(nh, rs) && nlf_id == -1 && ml_gid == -1; nh = NLMSG_NEXT(nh,rs))
		{
			if(nh->nlmsg_type != GENL_ID_CTRL)
			{
				continue;
			}
			char* nat = (char*)nh + sizeof(struct nlmsghdr) + sizeof(struct genlmsghdr);
			if(strncmp(nat + sizeof(struct nlattr), NL80211_GENL_NAME, sizeof(NL80211_GENL_NAME) - 1))
			{
				continue;
			}
			
			while(nat < (char*)nh + nh->nlmsg_len)
			{
				if(((struct nlattr*)nat)->nla_type == CTRL_ATTR_FAMILY_ID)
				{
					nlf_id = *(__u16*)(nat + sizeof(struct nlattr));
				}
				else if(((struct nlattr*)nat)->nla_type == CTRL_ATTR_MCAST_GROUPS)
				{
					char* wrap = nat + sizeof(struct nlattr);
					char* atr_nt = nat + 2 * sizeof(struct nlattr);
					size_t lim = NLA_ALIGN(((struct nlattr*)wrap)->nla_len);
					long tmp_id = -1;
					while(atr_nt < wrap + lim)
					{
						if(((struct nlattr*)atr_nt)->nla_type == CTRL_ATTR_MCAST_GRP_NAME)
						{
							if(!strncmp(atr_nt + sizeof(struct nlattr), 
										NL80211_MULTICAST_GROUP_MLME, sizeof(NL80211_MULTICAST_GROUP_MLME) - 1))
							{
								ml_gid = tmp_id == -1 
									? *(__u32*)(atr_nt + NLA_ALIGN(((struct nlattr*)atr_nt)->nla_len) + sizeof(struct nlattr)) 
									: tmp_id;
								break;
							}
							wrap += NLA_ALIGN(((struct nlattr*)wrap)->nla_len);
							atr_nt = wrap + sizeof(struct nlattr);
							lim = NLA_ALIGN(((struct nlattr*)wrap)->nla_len);
						}
						else if(((struct nlattr*)atr_nt)->nla_type == CTRL_ATTR_MCAST_GRP_ID)
						{
							tmp_id = *(__u32*)(atr_nt + sizeof(struct nlattr));
							atr_nt += NLA_ALIGN(((struct nlattr*)atr_nt)->nla_len);
						}
					}
				}
	
				if(nlf_id != -1 && ml_gid != -1)
				{
					break;
				}
				nat += NLA_ALIGN(((struct nlattr*)nat)->nla_len);
			}	
		}
		memset(buf, 0, rs);
	}
	
	struct 
	{
		struct nlmsghdr nh;
		struct genlmsghdr gnh;
		struct nlattr nat;
		__u32 ifind;

	} cc_buf = 
	{
		.nh = 
		{ 
			.nlmsg_len = sizeof(cc_buf),
			.nlmsg_type = (__u16)nlf_id,
			.nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP
		},
		.gnh = 
		{
			.cmd = NL80211_CMD_GET_STATION,
			.version = 1
		},
		.nat = 
		{
			.nla_len = sizeof(struct nlattr) + sizeof(__u32),
			.nla_type = NL80211_ATTR_IFINDEX
		}
	};
	
	int con = -1;
	char tmp_ssid[sizeof("Connected to ") + 32] = { 0 };
	long ifind = wlifi();
	if((cc_buf.ifind = ifind))
	{
		iov.iov_base = &cc_buf;
		iov.iov_len = sizeof(cc_buf);
		rs = 0;
		if(sendmsg(gns, &msg, 0) != -1)
		{
			memset(buf, 0, 8192);
			iov.iov_base = buf;
			iov.iov_len = 8192;
			while((rs = recvmsg(gns, &msg, 0)) != -1)
			{
				if(((struct sockaddr_nl*)msg.msg_name)->nl_pid && ((struct sockaddr_nl*)msg.msg_name)->nl_family != AF_NETLINK)
				{
					break;
				}	
				for(struct nlmsghdr* nh = (struct nlmsghdr*)buf; NLMSG_OK(nh, rs) && con == -1; nh = NLMSG_NEXT(nh,rs))
				{
					char* nat = (char*)nh + sizeof(struct nlmsghdr) + sizeof(struct genlmsghdr);
					while((nat < (char*)nh + nh->nlmsg_len))
					{
						if((((struct nlattr*)nat)->nla_type) == NL80211_ATTR_MAC)
						{
							char* mac = nat + sizeof(struct nlattr);
							con = (mac[0] || mac[1] || mac[2] || mac[3] || mac[4] || mac[5]);
							break;
						}
						nat += NLA_ALIGN(((struct nlattr*)nat)->nla_len);
					}
					if(con == -1)
					{
						con = 0;
					}	
				}
				memset(buf, 0, rs);
			}
		}	
	}

	if(setsockopt(gns, SOL_NETLINK, NETLINK_ADD_MEMBERSHIP, &ml_gid, sizeof(ml_gid)))
	{
		close(gns);
		return -1;
	}
	
	sigset_t orig, block;
	sigemptyset(&orig);
	sigemptyset(&block);
	sigaddset(&block, SIGALRM);
	sigaddset(&block, SIGHUP);
	sigaddset(&block, SIGINT);
	sigaddset(&block, SIGQUIT);
	sigaddset(&block, SIGTSTP);
	sigaddset(&block, SIGTERM);

	struct sigaction act = { .sa_handler = &sig_hand };
	if(sigaction(SIGALRM, &act, NULL) || sigaction(SIGHUP, &act, NULL) || sigaction(SIGINT, &act, NULL)
			|| sigaction(SIGQUIT, &act, NULL) || sigaction(SIGTSTP, &act, NULL) || sigaction(SIGTERM, &act, NULL))
	{
		close(gns);
		return -1;
	}

	if(pthread_sigmask(SIG_BLOCK, &block, &orig))
	{
		close(gns);
		return -1;
	}

	int eps = epoll_create1(EPOLL_CLOEXEC);
	if(eps < 0)
	{
		close(gns);
		return -1;
	}

	struct epoll_event ev = { .events = EPOLLIN | EPOLLET };
	if(epoll_ctl(eps, EPOLL_CTL_ADD, gns, &ev))
	{
		close(gns);
		close(eps);
		return -1;
	}	

	struct epoll_event rec_ev = { 0 };
	memset(buf, 0, 8192);
	rs = 0;
	char ssid_msg[sizeof("Connected to ") + 32] = { 0 };
	char ext = 0;
	__u16 dauth[] = { 2, 3, 4, 5, 6, 7, 8 };
	int wat = 0;
	int ep_fs = 0;
	while(atomic_load_explicit(&rfl, memory_order_acquire))
	{
		if((ep_fs = epoll_pwait(eps, &rec_ev, 1, -1, &orig)) == -1)
		{
			if(errno == EINTR)
			{
				continue;
			}
			break;
		}
		while((rs = recvmsg(gns, &msg, 0)) != -1)
		{
			if(((struct sockaddr_nl*)msg.msg_name)->nl_pid && ((struct sockaddr_nl*)msg.msg_name)->nl_family != AF_NETLINK)
			{
				continue;
			}
			for(struct nlmsghdr* nh = (struct nlmsghdr*)buf; NLMSG_OK(nh, rs) && !ext; nh = NLMSG_NEXT(nh,rs))
			{
				if(((struct genlmsghdr*)(buf + sizeof(struct nlmsghdr)))->cmd == NL80211_CMD_CONNECT && !con)
				{
					struct nlattr* nat = (struct nlattr*)((char*)nh + sizeof(struct nlmsghdr) + sizeof(struct genlmsghdr));
					while((char*)nat < (char*)nh + nh->nlmsg_len)
					{
						if(nat->nla_type == NL80211_ATTR_WIPHY)
						{
							con = 1;
							wat = 1;
						}
						else if(nat->nla_type == NL80211_ATTR_REQ_IE)
						{
							memset(ssid_msg, 0, sizeof("Connected to ") + 32);
							char* tlv = (char*)nat + sizeof(struct nlattr);
							while(*tlv != IEEE80211_WLAN_EID_SSID)
							{
								tlv += 2 + *(tlv + 1);
							}
							strcpy(ssid_msg, "Connected to ");
							strncpy(ssid_msg + sizeof("Connected to ") - 1, tlv + 2, *(tlv + 1));
							notify(ssid_msg);
							ext = 1;
							if(wat)
							{
								break;
							}
						}
						nat = (struct nlattr*)((char*)nat + NLA_ALIGN(nat->nla_len));
					}		
				}
				else if(((struct genlmsghdr*)(buf + sizeof(struct nlmsghdr)))->cmd == NL80211_CMD_DISCONNECT)
				{
					struct nlattr* nat = (struct nlattr*)((char*)nh + sizeof(struct nlmsghdr) + sizeof(struct genlmsghdr));
					while((char*)nat < (char*)nh + nh->nlmsg_len)
					{
						if(nat->nla_type == NL80211_ATTR_REASON_CODE)
						{
							__u16 reas = *(__u16*)((char*)nat + sizeof(struct nlattr));
							for(int i = 0; i < sizeof(dauth) / sizeof(__u16) && con; ++i)
							{
								con &= !(reas == dauth[i]);
							}
							break;
						}
						nat = (struct nlattr*)((char*)nat + NLA_ALIGN(nat->nla_len));
					}
					if(!con)
					{
						notify("Wifi disconnected");
						break;
					}
				}
			}
			ext = 0;
			memset(buf, 0, rs);
		}
		if(errno != EWOULDBLOCK && errno != EAGAIN)
		{
			break;
		}
	}
	
	close(gns);
	close(eps);
	return 0;
}

