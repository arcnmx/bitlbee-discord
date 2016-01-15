/*
 * Copyright 2015 Artem Savkov <artem.savkov@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "discord-util.h"

void free_user_info(user_info *uinfo)
{
  g_free(uinfo->name);
  g_free(uinfo->id);
  g_free(uinfo);
}

void free_channel_info(channel_info *cinfo)
{
  g_free(cinfo->id);
  cinfo->id = NULL;

  if (cinfo->type != CHANNEL_TEXT) {
    g_free(cinfo->to.handle.name);
  } else {
    imcb_chat_free(cinfo->to.channel.gc);
  }

  g_free(cinfo);
}

void free_server_info(server_info *sinfo)
{
  g_free(sinfo->name);
  g_free(sinfo->id);

  g_slist_free_full(sinfo->channels, (GDestroyNotify)free_channel_info);
  g_slist_free_full(sinfo->users, (GDestroyNotify)free_user_info);

  g_free(sinfo);
}

void free_gw_data(gw_data *gw)
{
  if (gw != NULL) {
    g_free(gw->addr);
    g_free(gw->path);

    g_free(gw);
  }
}

void free_discord_data(discord_data *dd)
{
  g_slist_free_full(dd->pchannels, (GDestroyNotify)free_channel_info);
  g_slist_free_full(dd->servers, (GDestroyNotify)free_server_info);

  free_gw_data(dd->gateway);
  g_free(dd->token);
  g_free(dd->uname);
  g_free(dd->id);

  g_free(dd);
}

static gint cmp_chan_id(const channel_info *cinfo, const char *chan_id)
{
  return g_strcmp0(cinfo->id, chan_id);
}

static gint cmp_user_id(const user_info *uinfo, const char *user_id)
{
  return g_strcmp0(uinfo->id, user_id);
}

static gint cmp_user_name(const user_info *uinfo, const char *uname)
{
  return g_strcmp0(uinfo->name, uname);
}

static gint cmp_user_name_ignorecase(const user_info *uinfo, const char *uname)
{
  gchar *cfn1 = g_utf8_casefold(uinfo->name, -1);
  gchar *cfn2 = g_utf8_casefold(uname, -1);
  gint result = g_strcmp0(cfn1, cfn2);

  g_free(cfn1);
  g_free(cfn2);
  return result;
}

static gint cmp_server_id(const server_info *sinfo, const char *server_id)
{
  return g_strcmp0(sinfo->id, server_id);
}

server_info *get_server_by_id(discord_data *dd, const char *server_id)
{
  GSList *sl = g_slist_find_custom(dd->servers, server_id,
                                   (GCompareFunc)cmp_server_id);

  return sl == NULL ?  NULL : sl->data;
}

channel_info *get_channel_by_id(discord_data *dd, const char *channel_id,
                                const char *server_id)
{
  GSList *cl = g_slist_find_custom(dd->pchannels, channel_id,
                                   (GCompareFunc)cmp_chan_id);

  if (cl == NULL) {
    if (server_id != NULL) {
      server_info *sinfo = get_server_by_id(dd, server_id);
      cl = g_slist_find_custom(sinfo->channels, channel_id,
                               (GCompareFunc)cmp_chan_id);
    } else {
      for (GSList *sl = dd->servers; sl; sl = g_slist_next(sl)) {
        server_info *sinfo = sl->data;
        cl = g_slist_find_custom(sinfo->channels, channel_id,
                                 (GCompareFunc)cmp_chan_id);
        if (cl != NULL) {
          break;
        }
      }
    }
  }

  return cl == NULL ?  NULL : cl->data;
}

user_info *get_user(discord_data *dd, const char *uname,
                    const char *server_id, search_t type)
{
  GSList *ul = NULL;
  GCompareFunc sfunc = NULL;

  switch(type) {
    case SEARCH_ID:
      sfunc = (GCompareFunc)cmp_user_id;
      break;
    case SEARCH_NAME:
      sfunc = (GCompareFunc)cmp_user_name;
      break;
    case SEARCH_NAME_IGNORECASE:
      sfunc = (GCompareFunc)cmp_user_name_ignorecase;
      break;
    default:
      return NULL;
  }

  if (server_id != NULL) {
    server_info *sinfo = get_server_by_id(dd, server_id);
    ul = g_slist_find_custom(sinfo->users, uname, sfunc);
  } else {
    for (GSList *sl = dd->servers; sl; sl = g_slist_next(sl)) {
      server_info *sinfo = sl->data;
      ul = g_slist_find_custom(sinfo->users, uname, sfunc);
      if (ul != NULL) {
        break;
      }
    }
  }

  return ul == NULL ?  NULL : ul->data;
}

char *discord_canonize_name(const char *name)
{
  GRegex *regex = g_regex_new("[@+ ]", 0, 0, NULL);
  char *cname = g_regex_replace_literal(regex, name, -1, 0, "_", 0, NULL);

  g_regex_unref(regex);
  return cname;
}
