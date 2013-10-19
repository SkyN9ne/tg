#include <assert.h>
#include "structures.h"
#include "mtproto-common.h"
#include "telegram.h"
#include "tree.h"
#include "loop.h"
int verbosity;

void fetch_file_location (struct file_location *loc) {
  int x = fetch_int ();
  if (x == CODE_file_location_unavailable) {
    loc->dc = -1;
    loc->volume = fetch_long ();
    loc->local_id = fetch_int ();
    loc->secret = fetch_long ();
  } else {
    assert (x == CODE_file_location);
    loc->dc = fetch_int ();;
    loc->volume = fetch_long ();
    loc->local_id = fetch_int ();
    loc->secret = fetch_long ();
  }
}

void fetch_user_status (struct user_status *S) {
  int x = fetch_int ();
  switch (x) {
  case CODE_user_status_empty:
    S->online = 0;
    break;
  case CODE_user_status_online:
    S->online = 1;
    S->when = fetch_int ();
    break;
  case CODE_user_status_offline:
    S->online = -1;
    S->when = fetch_int ();
    break;
  default:
    assert (0);
  }
}

int our_id;
void fetch_user (struct user *U) {
  memset (U, 0, sizeof (*U));
  unsigned x = fetch_int ();
  assert (x == CODE_user_empty || x == CODE_user_self || x == CODE_user_contact ||  x == CODE_user_request || x == CODE_user_foreign || x == CODE_user_deleted);
  U->id = fetch_int ();
  if (x == CODE_user_empty) {
    U->flags = 1;
    return;
  }
  if (x == CODE_user_self) {
    assert (!our_id || (our_id == U->id));
    if (!our_id) {
      our_id = U->id;
      write_auth_file ();
    }
  }
  U->first_name = fetch_str_dup ();
  U->last_name = fetch_str_dup ();
  if (!strlen (U->first_name)) {
    if (!strlen (U->last_name)) {
      U->print_name = strdup ("none");
    } else {
      U->print_name = strdup (U->last_name);
    }
  } else {
    if (!strlen (U->last_name)) {
      U->print_name = strdup (U->first_name);
    } else {
      U->print_name = malloc (strlen (U->first_name) + strlen (U->last_name) + 2);
      sprintf (U->print_name, "%s_%s", U->first_name, U->last_name);
    }
  }
  char *s = U->print_name;
  while (*s) {
    if (*s == ' ') { *s = '_'; }
    s++;
  }
  if (x == CODE_user_deleted) {
    U->flags = 2;
    return;
  }
  if (x == CODE_user_self) {
    U->flags = 4;
  } else {
    U->access_hash = fetch_long ();
  }
  if (x == CODE_user_foreign) {
    U->flags |= 8;
  } else {
    U->phone = fetch_str_dup ();
  }
  unsigned y = fetch_int ();
  if (y == CODE_user_profile_photo_empty) {
    U->photo_small.dc = -2;
    U->photo_big.dc = -2;
  } else {
    assert (y == CODE_user_profile_photo);
    fetch_file_location (&U->photo_small);
    fetch_file_location (&U->photo_big);
  }
  fetch_user_status (&U->status);
  if (x == CODE_user_self) {
    assert (fetch_int () == (int)CODE_bool_false);
  }
  if (x == CODE_user_contact) {
    U->flags |= 16;
  }
}

void fetch_chat (struct chat *C) {
  memset (C, 0, sizeof (*C));
  unsigned x = fetch_int ();
  assert (x == CODE_chat_empty || x == CODE_chat || x == CODE_chat_forbidden);
  C->id = -fetch_int ();
  if (x == CODE_chat_empty) {
    C->flags = 1;
    return;
  }
  if (x == CODE_chat_forbidden) {
    C->flags |= 8;
  }
  C->title = fetch_str_dup ();
  C->print_title = strdup (C->title);
  char *s = C->print_title;
  while (*s) {
    if (*s == ' ') { *s = '_'; }
    s ++;
  }
  if (x == CODE_chat) {
    unsigned y = fetch_int ();
    if (y == CODE_chat_photo_empty) {
      C->photo_small.dc = -2;
      C->photo_big.dc = -2;
    } else {
      assert (y == CODE_chat_photo);
      fetch_file_location (&C->photo_small);
      fetch_file_location (&C->photo_big);
    }
    C->user_num = fetch_int ();
    C->date = fetch_int ();
    if (fetch_int () == (int)CODE_bool_true) {
      C->flags |= 16;
    }
    C->version = fetch_int ();
  } else {
    C->photo_small.dc = -2;
    C->photo_big.dc = -2;
    C->user_num = -1;
    C->date = fetch_int ();
    C->version = -1;
  }
}

void fetch_photo_size (struct photo_size *S) {
  memset (S, 0, sizeof (*S));
  unsigned x = fetch_int ();
  assert (x == CODE_photo_size || x == CODE_photo_cached_size);
  S->type = fetch_str_dup ();
  fetch_file_location (&S->loc);
  S->w = fetch_int ();
  S->h = fetch_int ();
  if (x == CODE_photo_size) {
    S->size = fetch_int ();
  } else {
    S->data = fetch_str_dup ();
  }
}

void fetch_geo (struct geo *G) {
  unsigned x = fetch_int ();
  if (x == CODE_geo_point) {
    G->longitude = fetch_double ();
    G->latitude = fetch_double ();
  } else {
    assert (x == CODE_geo_point_empty);
    G->longitude = 0;
    G->latitude = 0;
  }
}

void fetch_photo (struct photo *P) {
  memset (P, 0, sizeof (*P));
  unsigned x = fetch_int ();
  P->id = fetch_long ();
  if (x == CODE_photo_empty) { return; }
  P->access_hash = fetch_long ();
  P->user_id = fetch_int ();
  P->date = fetch_int ();
  P->caption = fetch_str_dup ();
  fetch_geo (&P->geo);
  assert (fetch_int () == CODE_vector);
  P->sizes_num = fetch_int ();
  P->sizes = malloc (sizeof (struct photo_size) * P->sizes_num);
  int i;
  for (i = 0; i < P->sizes_num; i++) {
    fetch_photo_size (&P->sizes[i]);
  }
}

void fetch_video (struct video *V) {
  memset (V, 0, sizeof (*V));
  unsigned x = fetch_int ();
  V->id = fetch_long ();
  if (x == CODE_video_empty) { return; }
  V->access_hash = fetch_long ();
  V->user_id = fetch_int ();
  V->date = fetch_int ();
  V->caption = fetch_str_dup ();
  V->duration = fetch_int ();
  V->size = fetch_int ();
  fetch_photo_size (&V->thumb);
  V->dc_id = fetch_int ();
  V->w = fetch_int ();
  V->h = fetch_int ();
}

void fetch_message_action (struct message_action *M) {
  memset (M, 0, sizeof (*M));
  unsigned x = fetch_int ();
  M->type = x;
  switch (x) {
  case CODE_message_action_empty:
    break;
  case CODE_message_action_chat_create:
    M->title = fetch_str_dup ();
    assert (fetch_int () == (int)CODE_vector);
    M->user_num = fetch_int ();
    M->users = malloc (M->user_num * 4);
    fetch_ints (M->users, M->user_num);
    break;
  case CODE_message_action_chat_edit_title:
    M->new_title = fetch_str_dup ();
    break;
  case CODE_message_action_chat_edit_photo:
    fetch_photo (&M->photo);
    break;
  case CODE_message_action_chat_delete_photo:
    break;
  case CODE_message_action_chat_add_user:
    M->user = fetch_int ();
    break;
  case CODE_message_action_chat_delete_user:
    M->user = fetch_int ();
    break;
  default:
    assert (0);
  }
}

void fetch_message_short (struct message *M) {
  memset (M, 0, sizeof (*M));
  M->id = fetch_int ();
  M->from_id = fetch_int ();
  M->message = fetch_str_dup ();
  fetch_int (); // pts
  M->date = fetch_int ();
  fetch_int (); // seq
  M->media.type = CODE_message_media_empty;
}

void fetch_message_short_chat (struct message *M) {
  memset (M, 0, sizeof (*M));
  M->id = fetch_int ();
  M->from_id = fetch_int ();
  M->to_id = -fetch_int ();
  M->message = fetch_str_dup ();
  fetch_int (); // pts
  M->date = fetch_int ();
  fetch_int (); // seq
  M->media.type = CODE_message_media_empty;
}


void fetch_message_media (struct message_media *M) {
  memset (M, 0, sizeof (*M));
  M->type = fetch_int ();
  switch (M->type) {
  case CODE_message_media_empty:
    break;
  case CODE_message_media_photo:
    fetch_photo (&M->photo);
    break;
  case CODE_message_media_video:
    fetch_video (&M->video);
    break;
  case CODE_message_media_geo:
    fetch_geo (&M->geo);
    break;
  case CODE_message_media_contact:
    M->phone = fetch_str_dup ();
    M->first_name = fetch_str_dup ();
    M->last_name = fetch_str_dup ();
    M->user_id = fetch_int ();
    break;
  case CODE_message_media_unsupported:
    M->data = fetch_str_dup ();
    break;
  default:
    logprintf ("type = 0x%08x\n", M->type);
    assert (0);
  }
}

int fetch_peer_id (void) {
  unsigned x =fetch_int ();
  if (x == CODE_peer_user) {
    return fetch_int ();
  } else {
    assert (CODE_peer_chat);
    return -fetch_int ();
  }
}

void fetch_message (struct message *M) {
  memset (M, 0, sizeof (*M));
  unsigned x = fetch_int ();
  assert (x == CODE_message_empty || x == CODE_message || x == CODE_message_forwarded || x == CODE_message_service);
  M->id = fetch_int ();
  if (x == CODE_message_empty) {
    M->flags |= 1;
    return;
  }
  if (x == CODE_message_forwarded) {
    M->fwd_from_id = fetch_int ();
    M->fwd_date = fetch_int ();
  }
  M->from_id = fetch_int ();
  M->to_id = fetch_peer_id ();
  M->out = fetch_bool ();
  M->unread = fetch_bool ();
  M->date = fetch_int ();
  if (x == CODE_message_service) {
    M->service = 1;
    fetch_message_action (&M->action);
  } else {
    M->message = fetch_str_dup ();
    fetch_message_media (&M->media);
  }
}

#define user_cmp(a,b) ((a)->id - (b)->id)

DEFINE_TREE(peer,union user_chat *,user_cmp,0)
DEFINE_TREE(message,struct message *,user_cmp,0)
struct tree_peer *peer_tree;
struct tree_message *message_tree;

int chat_num;
int user_num;
int users_allocated;
int chats_allocated;
int messages_allocated;
union user_chat *Peers[MAX_USER_NUM];

struct message message_list = {
  .next_use = &message_list,
  .prev_use = &message_list
};

struct user *fetch_alloc_user (void) {
  union user_chat *U = malloc (sizeof (*U));
  fetch_user (&U->user);
  users_allocated ++;
  union user_chat *U1 = tree_lookup_peer (peer_tree, U);
  if (U1) {
    free_user (&U1->user);
    memcpy (U1, U, sizeof (*U));
    free (U);
    users_allocated --;
    return &U1->user;
  } else {
    peer_tree = tree_insert_peer (peer_tree, U, lrand48 ());
    Peers[chat_num + (user_num ++)] = U;
    return &U->user;
  }
}

void free_user (struct user *U) {
  if (U->first_name) { free (U->first_name); }
  if (U->last_name) { free (U->last_name); }
  if (U->print_name) { free (U->print_name); }
  if (U->phone) { free (U->phone); }
}

void free_photo_size (struct photo_size *S) {
  free (S->type);
  if (S->data) {
    free (S->data);
  }
}

void free_photo (struct photo *P) {
  if (!P->access_hash) { return; }
  if (P->caption) { free (P->caption); }
  if (P->sizes) {
    int i;
    for (i = 0; i < P->sizes_num; i++) {
      free_photo_size (&P->sizes[i]);
    }
    free (P->sizes);
  }
}

void free_video (struct video *V) {
  if (!V->access_hash) { return; }
  free (V->caption);
  free_photo_size (&V->thumb);
}

void free_message_media (struct message_media *M) {
  switch (M->type) {
  case CODE_message_media_empty:
  case CODE_message_media_geo:
    return;
  case CODE_message_media_photo:
    free_photo (&M->photo);
    return;
  case CODE_message_media_video:
    free_video (&M->video);
    return;
  case CODE_message_media_contact:
    free (M->phone);
    free (M->first_name);
    free (M->last_name);
    return;
  case CODE_message_media_unsupported:
    free (M->data);
    return;
  default:
    logprintf ("%08x\n", M->type);
    assert (0);
  }
}

void free_message_action (struct message_action *M) {
  switch (M->type) {
  case CODE_message_action_empty:
    break;
  case CODE_message_action_chat_create:
    free (M->title);
    free (M->users);
    break;
  case CODE_message_action_chat_edit_title:
    free (M->new_title);
    break;
  case CODE_message_action_chat_edit_photo:
    free_photo (&M->photo);
    break;
  case CODE_message_action_chat_delete_photo:
    break;
  case CODE_message_action_chat_add_user:
    break;
  case CODE_message_action_chat_delete_user:
    break;
  default:
    assert (0);
  }
}

void free_message (struct message *M) {
  if (!M->service) {
    if (M->message) { free (M->message); }
    free_message_media (&M->media);
  } else {
    free_message_action (&M->action);
  }
}

void message_del_use (struct message *M) {
  M->next_use->prev_use = M->prev_use;
  M->prev_use->next_use = M->next_use;
}

void message_add_use (struct message *M) {
  M->next_use = message_list.next_use;
  M->prev_use = &message_list;
  M->next_use->prev_use = M;
  M->prev_use->next_use = M;
}

struct message *fetch_alloc_message (void) {
  struct message *M = malloc (sizeof (*M));
  fetch_message (M);
  struct message *M1 = tree_lookup_message (message_tree, M);
  messages_allocated ++;
  if (M1) {
    message_del_use (M1);
    free_message (M1);
    memcpy (M1, M, sizeof (*M));
    free (M);
    message_add_use (M1);
    messages_allocated --;
    return M1;
  } else {
    message_add_use (M);
    message_tree = tree_insert_message (message_tree, M, lrand48 ());
    return M;
  }
}

struct message *fetch_alloc_message_short (void) {
  struct message *M = malloc (sizeof (*M));
  fetch_message_short (M);
  struct message *M1 = tree_lookup_message (message_tree, M);
  messages_allocated ++;
  if (M1) {
    message_del_use (M1);
    free_message (M1);
    memcpy (M1, M, sizeof (*M));
    free (M);
    message_add_use (M1);
    messages_allocated --;
    return M1;
  } else {
    message_add_use (M);
    message_tree = tree_insert_message (message_tree, M, lrand48 ());
    return M;
  }
}

struct message *fetch_alloc_message_short_chat (void) {
  struct message *M = malloc (sizeof (*M));
  fetch_message_short_chat (M);
  if (verbosity >= 2) {
    logprintf ("Read message with id %d\n", M->id);
  }
  struct message *M1 = tree_lookup_message (message_tree, M);
  messages_allocated ++;
  if (M1) {
    message_del_use (M1);
    free_message (M1);
    memcpy (M1, M, sizeof (*M));
    free (M);
    message_add_use (M1);
    messages_allocated --;
    return M1;
  } else {
    message_add_use (M);
    message_tree = tree_insert_message (message_tree, M, lrand48 ());
    return M;
  }
}

struct chat *fetch_alloc_chat (void) {
  union user_chat *U = malloc (sizeof (*U));
  fetch_chat (&U->chat);
  chats_allocated ++;
  union user_chat *U1 = tree_lookup_peer (peer_tree, U);
  if (U1) {
    free_chat (&U1->chat);
    *U1 = *U;
    free (U);
    chats_allocated --;
    return &U1->chat;
  } else {
    peer_tree = tree_insert_peer (peer_tree, U, lrand48 ());
    Peers[(chat_num ++) + user_num] = U;
    return &U->chat;
  }
}

void free_chat (struct chat *U) {
  if (U->title) { free (U->title); }
  if (U->print_title) { free (U->print_title); }
}

int print_stat (char *s, int len) {
  return snprintf (s, len, 
    "users_allocated\t%d\n"
    "chats_allocated\t%d\n"
    "messages_allocated\t%d\n",
    users_allocated,
    chats_allocated,
    messages_allocated
    );
}

union user_chat *user_chat_get (int id) {
  union user_chat U;
  U.id = id;
  return tree_lookup_peer (peer_tree, &U);
}

struct message *message_get (int id) {
  struct message M;
  M.id = id;
  return tree_lookup_message (message_tree, &M);
}

void update_message_id (struct message *M, int id) {
  message_tree = tree_delete_message (message_tree, M);
  M->id = id;
  message_tree = tree_insert_message (message_tree, M, lrand48 ());
}

void message_insert (struct message *M) {
  message_add_use (M);
  message_tree = tree_insert_message (message_tree, M, lrand48 ());
}
