/*
 * request.c
 *
 * Copyright (c) 1995 Erik Corry ehcorry@inet.uni-c.dk
 * Copyright (c) 1996-1997 John Kilburg <john@cs.unlv.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include "port_before.h"

#include <stdio.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#ifdef HAVE_UNAME
#include <sys/utsname.h>
#endif

#include <X11/IntrinsicP.h>

#include "port_after.h"

#include "ChimeraP.h"

static bool match_no_proxy _ArgProto((char *, char *));
static bool match_no_proxy_list _ArgProto((char *, char *, char *, char *));
static bool match_host_name _ArgProto((char *, char *));
static URLParts *get_proxy _ArgProto((MemPool, URLParts *));

static bool
match_no_proxy(remote_host, no_proxy_domain)
char *remote_host;
char *no_proxy_domain;
{
  int remote_len = strlen(remote_host);
  int no_proxy_len = strlen(no_proxy_domain);
  int min_len = MIN(remote_len, no_proxy_len);

  /*
   * avoid matching parts of domain names not divided by dots.
   */
  if(remote_len < no_proxy_len)
    if(no_proxy_domain[no_proxy_len - min_len - 1] != '.')
      return false;
  if(remote_len > no_proxy_len)
    if(remote_host[remote_len - min_len - 1] != '.')
      return false;

  /*
   * Do ends of strings match?
   */
  if (strncmp(remote_host + remote_len - min_len,
              no_proxy_domain + no_proxy_len - min_len,
              min_len) == 0)
    return(true);
  else
    return(false);
}

/*
 * If no_proxy is not defined, then use the part that is common
 * between the proxy server's name and the uname of the current host.
 * I.e. if the proxy server is wwwproxy.firewall.dreadco.com, and the
 * local host is dilbert-sparc.widget-dept.dreadco.com, then the proxy
 * is not used for *.dreadco.com addresses. Or if your proxy is set to
 * http://wwwcache.hensa.ac.uk#8080/ and your machine is aiai.ed.ac.uk,
 * then ac.uk addresses will not go via the cache.
 */
static bool
match_no_proxy_list(remote_host, local_host, no_proxy_list, proxy_host)
char *remote_host;
char *local_host;
char *no_proxy_list;
char *proxy_host;
{
  if (no_proxy_list != NULL)
  {
    char *local_no_proxy_list;
    char *no_proxy_elt;
    MemPool mp;
    int i;
    int l = strlen(no_proxy_list);

    mp = MPCreate();

    /*
     * Step through comma-separated list of domains/machines for
     * which no proxy should be used
     */
    no_proxy_elt = local_no_proxy_list = (char *)MPGet(mp, l + 1);
    for (i = 0; i <= l; i++)
    {
      char temp;
      if ((temp = local_no_proxy_list[i] = no_proxy_list[i]) == ',' ||
          temp == 0)
      {
        /* Zero out comma in copy of list */
        local_no_proxy_list[i] = 0;
        if(match_no_proxy(remote_host, no_proxy_elt))
        {
	  MPDestroy(mp);
          return true;
        }
        /* advance pointer to next element in copy of list */
        no_proxy_elt = local_no_proxy_list + i + 1;
      }
    }
    MPDestroy(mp);
    return(false);
  }
  else
  {
    /*
     * Use default no_proxy. See comments above.
     */
    int i, j;
    int l, pl;
    if(!local_host) return false;
    l = strlen(local_host);
    pl = strlen(proxy_host);
    for (i = l - 1, j = pl -1; i && j; i--, j--)
    {
      if(local_host[i] != proxy_host[i])
      {
        /* We have found the first point where local_host and proxy_host
         * differ (searching back from end). Now find the first '.' in
         * the common suffix, since we don't want to match partial
         * names
         */
         for(; i < l; i++, j++)
           if(local_host[i] == '.') break;

         /* If the local_host and the proxy_host names don't have
          * an parts in common, then use the proxy, ie no match
          * with no_proxy, ie return false
          */
         if(local_host[i] == 0)
           return false;

         /* Now local_host + i points to default no_proxy value */
         return(match_no_proxy(remote_host, local_host+i));
      }

    }
    /* If we are here, then the entire string matched, Ie we are using
     * the local host as a proxy. In that case always use the proxy, ie
     * report no match, ie false
     */
    return(false);
  }
}

/*
 * Are they the same host? For example kroete2.freinet.de is the same as
 * kroete2. Doesn't match kgb.ussr with kgb.us
 */
static bool
match_host_name(remote_host, local_host)
char *remote_host;
char *local_host;
{
  int rl = strlen(remote_host);
  int ll = strlen(local_host);
  /* Same length, same string */
  if(ll == rl) return(strcmp(remote_host, local_host) == 0 ? true : false);
  if(ll < rl)
  {
    /*
     * kroete2. matches kroete2.freinet.de and
     * kroete2 matches kroete2.freinet.de
     */
    if(local_host[ll - 1] == '.' || remote_host[ll] == '.')
    {
      return(strncmp(remote_host, local_host, ll) == 0 ? true : false);
    }
    return(false);
  }
  else
  {
    /*
     * kroete2. matches kroete2.freinet.de and
     * kroete2 matches kroete2.freinet.de
     */
    if(remote_host[rl - 1] == '.' || local_host[rl] == '.')
    {
      return(strncmp(remote_host, local_host, rl) == 0 ? true : false);
    }
    return(false);
  }
}

/*
 * get_proxy
 *
 * There is no proxy for 'localhost'.
 * There is also no proxy for the local system name (as
 * returned by uname(2) or 'uname -n'). Otherwise do the
 * no_proxy checking.
 *
 * I scratched around here...so this is my fault. -john
 */
static URLParts *
get_proxy(mp, up)
MemPool mp;
URLParts *up;
{
  URLParts *pup = NULL;
  char *pname;
  char *apl;
# ifdef HAVE_UNAME
  struct utsname thishostname;
# endif

  if (up->scheme == NULL) return(NULL);

  /*
   * Check for protocol specific proxy information.
   */
  pname = (char *)MPGet(mp, strlen(up->scheme) + strlen("_proxy") + 1);
  strcpy(pname, up->scheme);
  strcat(pname, "_proxy");
  if (getenv(pname) != NULL) pup = URLParse(mp, getenv(pname));

  /*
   * Check for "all" proxy information.
   */
  if (pup == NULL)
  {
    /*
     * Check the all list to make sure that the protocol should be
     * used with "all".  If the list doesn't exist then just assume
     * "all" really means "all".
     */
    if ((apl = getenv("all_proxy_list")) != NULL)
    {
      pname = apl;
      while ((pname = mystrtok(apl, ',', &apl)) != NULL)
      {
	if (strlen(up->scheme) == strlen(pname) &&
	    strcmp(up->scheme, pname) == 0) break;
      }
      if (pname == NULL) return(NULL);
    }
    if (getenv("all_proxy") != NULL) pup = URLParse(mp, getenv("all_proxy"));
  }

  /*
   * No proxy information?
   */
  if (pup == NULL) return(NULL);

  /*
   * Found proxy information now make sure that it can be used.
   */
# ifdef HAVE_UNAME
  uname(&thishostname);
  if (up->hostname &&
/*
 * Allow localhost and hostname proxies
 * 
      strcmp(up->hostname, "localhost") != 0 &&
      ! match_host_name(up->hostname, thishostname.nodename) &&
*/
      ! match_no_proxy_list(up->hostname,
			    thishostname.nodename,
			    getenv("no_proxy"),
			    pup->hostname))
  {
    return(pup);
  }
# else
  if (up->hostname &&
/*
 * Allow localhost and hostname proxies
 *
      strcmp(up->hostname, "localhost") != 0 &&
*/
      ! match_no_proxy_list(up->hostname,
			    NULL,
			    getenv("no_proxy"),
			    pup->hostname))
  {
    return(pup);
  }
# endif

  return(NULL);
}

/*
 * RequestMatchContent
 *
 * This is stupid.  I swear its because I'm tired.  Really.
 */
bool
RequestMatchContent(mp, c1, c2)
MemPool mp;
char *c1, *c2;
{
  char *cp;
  char *p1, *p2;
  bool error;

  c1 = MPStrDup(mp, c1);
  c2 = MPStrDup(mp, c2);
 
  error = true;
  for (cp = c1; *cp != '\0'; cp++)
  {
    if (*cp == '/')
    {
      p1 = cp + 1;
      *cp = '\0';
      error = false;
      break;
    }
  }
  if (error) return(false);

  error = true;
  for (cp = c2; *cp != '\0'; cp++)
  {
    if (*cp == '/')
    {
      p2 = cp + 1;
      *cp = '\0';
      error = false;
      break;
    }
  }
  if (error) return(false);

  if ((strlen(c1) == 1 && *c1 == '*') || 
      (strlen(c1) == strlen(c2) && strcasecmp(c1, c2) == 0))
  {
    if ((strlen(p1) == 1 && *p1 == '*') || 
        (strlen(p1) == strlen(p2) && strcasecmp(p1, p2) == 0))
    {
      return(true);
    }
  }

  return(false);
}

/*
 * RequestCreate
 *
 * Creates a request that can be passed to a source initializer.
 *
 * If the base is NULL and the URL is relative then it is assumed to
 * be a domain-name which is transformed to http://domain-name/.
 */
ChimeraRequest *
RequestCreate(cres, url, base)
ChimeraResources cres;
char *url;
char *base;
{
  MemPool mp;
  ChimeraRequest *wr, *tr;
  ChimeraSourceHooks *hooks, *phooks;
  URLParts *up;      /* request url parsed */
  URLParts *bup;     /* base url parsed */
  URLParts *rup;     /* result url parsed */
  char *rurl;        /* result url */
  char *uscheme;     /* url scheme */
  char *bscheme;     /* base scheme */
  char *jscheme;     /* temp/junk scheme */
  char *turl;        /* temp url */
  size_t tlen;       /* length of turl */

  /* really, really want a URL */
  myassert(url != NULL, "URL passed to RequestCreate is NULL");

  /*
   * All allocations are done with this descriptor.
   */
  mp = MPCreate();
  wr = (ChimeraRequest *)MPCGet(mp, sizeof(ChimeraRequest));
  wr->mp = mp;
  if (base != NULL) wr->parent_url = MPStrDup(wr->mp, base);

  /*
   * First, get the schemes of the request URL and base URL.  It should
   * always be possible to get the schemes so that the source hooks
   * can be found to see if an alternate parsing method is going to
   * be used.
   */ 
  uscheme = URLGetScheme(mp, url);
  if (base != NULL) bscheme = URLGetScheme(mp, base);
  else bscheme = NULL;

  /*
   * Neither URL is absolute.  Pretend that the supplied URL is a domain
   * name if it doesn't begin with a '/'.
   */
  if (uscheme == NULL && bscheme == NULL)
  {
    if (url[0] != '/')
    {
      tlen = strlen(url) + strlen("http:///") + 1;
      turl = (char *)MPCGet(mp, tlen);
      snprintf (turl, tlen - 1, "http://%s/", url);
      tr = RequestCreate(cres, turl, NULL);
    }
    else tr = NULL;
    MPDestroy(mp);
    return(tr);
  }

  /*
   * Select the scheme to use.  If the request url doesn't have a scheme
   * then it must relative...use the base url scheme.
   */
  if (uscheme == NULL) jscheme = bscheme;
  else
  {
    jscheme = uscheme;

    /*
     * If the request url and base url don't have the same scheme then
     * ignore the base url (set it to NULL).
     */
    if (bscheme == NULL || strlen(uscheme) != strlen(bscheme) ||
	strcasecmp(uscheme, bscheme) != 0)
    {
      base = NULL;
    }
  }

  /*
   * If source hooks can't be found or the hooks don't define a resolver
   * then use the general resolver.
   */
  if ((hooks = SourceGetHooks(cres, jscheme)) != NULL &&
      hooks->resolve_url != NULL)
  {
    /*
     * Resolve the URL.  If it succeeds parse the URL since we need
     * that although that is probably the wrong thing to do because
     * the scheme required a unique resolver.  Shouldn't hurt unless
     * a proxy is needed.
     */
    if ((rurl = CMethodCharPtr(hooks->resolve_url)(mp, url, base)) == NULL)
    {
      MPDestroy(mp);
      return(NULL);
    }
    else rup = URLParse(mp, rurl);
  }
  else
  {
    /*
     * Try using the general parser.
     */
    if ((up = URLParse(mp, url)) == NULL)
    {
      MPDestroy(mp);
      return(NULL);
    }
    if (base != NULL && (bup = URLParse(mp, base)) != NULL)
    {
      if ((rup = URLResolve(mp, up, bup)) == NULL)
      {
	MPDestroy(mp);
	return(NULL);
      }
    }
    else rup = up;

    rurl = URLMakeString(mp, rup, true);
  }

  /*
   * At this point, "rurl" should contain the text of the URL allocated with
   * "mp".  "rup" should contain the parsed form of the URL which at least
   * gives a scheme.
   */
  wr->url = rurl;
  wr->up = rup;

  /*
   * Check for a proxy server.
   */
  if ((wr->pup = get_proxy(wr->mp, wr->up)) != NULL)
  {
    if ((phooks = SourceGetHooks(cres, wr->pup->scheme)) == NULL)
    {
      wr->pup = NULL;
    }
    else hooks = phooks;
  }

  /*
   * No hooks at all?  No choice but to fail.
   */
  if (hooks == NULL)
  {
    MPDestroy(mp);
    return(NULL);
  }

  memcpy(&(wr->hooks), hooks, sizeof(ChimeraSourceHooks));      

  wr->scheme = URLGetScheme(wr->mp, wr->url);

  return(wr);
}

void
RequestAddContent(wr, content)
ChimeraRequest *wr;
char *content;
{
  if (wr->contents == NULL) wr->contents = GListCreateX(wr->mp);
  GListAddTail(wr->contents, MPStrDup(wr->mp, content));
  return;
}

int
RequestAddRegexContent(cres, wr, ctx)
ChimeraResources cres;
ChimeraRequest *wr;
char *ctx;
{
  ChimeraRenderHooks *rh;
  GList list;
  int count = 0;
  MemPool mp;

  if (wr->contents == NULL) wr->contents = GListCreateX(wr->mp);

  mp = MPCreate();
  list = cres->renderhooks;
  for (rh = (ChimeraRenderHooks *)GListGetHead(list); rh != NULL;
       rh = (ChimeraRenderHooks *)GListGetNext(list))
  {
    if (RequestMatchContent(mp, ctx, rh->content))
    {
      GListAddTail(wr->contents, MPStrDup(wr->mp, rh->content));
      count++;
    }
  }
  MPDestroy(mp);

  return(count);
}

bool
RequestMatchContent2(wr, ct)
ChimeraRequest *wr;
char *ct;
{
  char *rx;
  GList list;
  MemPool mp;

  if (wr->contents == NULL || GListEmpty(wr->contents)) return(true);

  mp = MPCreate();

  list = wr->contents;
  for (rx = (char *)GListGetHead(list); rx != NULL;
       rx = (char *)GListGetNext(list))
  {
    if (RequestMatchContent(mp, rx, ct))
    {
      MPDestroy(mp);
      return(true);
    }
  }

  MPDestroy(mp);

  return(false);
}

void
RequestDestroy(wr)
ChimeraRequest *wr;
{
  MPDestroy(wr->mp);
  return;
}

void
RequestReload(wr, reload)
ChimeraRequest *wr;
bool reload;
{
  wr->reload = reload;
  return;
}

bool
RequestCompareURL(wr1, wr2)
ChimeraRequest *wr1, *wr2;
{
  /*
   * The comparison should ignore case in the host part and pay attention
   * to case in the filename part...
   */
  return(strlen(wr1->url) == strlen(wr2->url) &&
	 strcmp(wr1->url, wr2->url) == 0);
}

bool
RequestCompareAccept(wr1, wr2)
ChimeraRequest *wr1, *wr2;
{
  char *c1, *c2;

  c1 = (char *)GListGetHead(wr1->contents);
  c2 = (char *)GListGetHead(wr2->contents);
  while (c1 != NULL && c2 != NULL)
  {
    if (strlen(c1) != strlen(c2) || strcasecmp(c1, c2) != 0) return(false);

    c1 = (char *)GListGetNext(wr1->contents);
    c2 = (char *)GListGetNext(wr2->contents);
  }
  if (c1 != NULL || c2 != NULL) return(false);

  return(true);
}
