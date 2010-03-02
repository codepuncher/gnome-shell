/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/**
 * SECTION:shell-generic-container
 * @short_description: A container class with signals for allocation
 *
 * #ShellGenericContainer is mainly a workaround for the current
 * lack of GObject subclassing + vfunc overrides in gjs.  We
 * implement the container interface, but proxy the virtual functions
 * into signals, which gjs can catch.
 *
 * #ShellGenericContainer is an #StWidget, and automatically takes its
 * borders and padding into account during size request and allocation.
 */

#include "config.h"

#include "shell-generic-container.h"

#include <clutter/clutter.h>
#include <gtk/gtk.h>
#include <girepository.h>

static void shell_generic_container_iface_init (ClutterContainerIface *iface);

G_DEFINE_TYPE_WITH_CODE(ShellGenericContainer,
                        shell_generic_container,
                        ST_TYPE_WIDGET,
                        G_IMPLEMENT_INTERFACE (CLUTTER_TYPE_CONTAINER,
                                               shell_generic_container_iface_init));

struct _ShellGenericContainerPrivate {
  GList *children;
  GHashTable *skip_paint;
};

/* Signals */
enum
{
  GET_PREFERRED_WIDTH,
  GET_PREFERRED_HEIGHT,
  ALLOCATE,
  LAST_SIGNAL
};

static guint shell_generic_container_signals [LAST_SIGNAL] = { 0 };

static gpointer
shell_generic_container_allocation_ref (ShellGenericContainerAllocation *alloc)
{
  alloc->_refcount++;
  return alloc;
}

static void
shell_generic_container_allocation_unref (ShellGenericContainerAllocation *alloc)
{
  if (--alloc->_refcount == 0)
    g_slice_free (ShellGenericContainerAllocation, alloc);
}

static void
shell_generic_container_allocate (ClutterActor           *self,
                                  const ClutterActorBox  *box,
                                  ClutterAllocationFlags  flags)
{
  StThemeNode *theme_node;
  ClutterActorBox content_box;

  CLUTTER_ACTOR_CLASS (shell_generic_container_parent_class)->allocate (self, box, flags);

  theme_node = st_widget_get_theme_node (ST_WIDGET (self));
  st_theme_node_get_content_box (theme_node, box, &content_box);

  g_signal_emit (G_OBJECT (self), shell_generic_container_signals[ALLOCATE], 0,
                 &content_box, flags);
}

static void
shell_generic_container_get_preferred_width (ClutterActor *actor,
                                             gfloat        for_height,
                                             gfloat       *min_width_p,
                                             gfloat       *natural_width_p)
{
  ShellGenericContainerAllocation *alloc = g_slice_new0 (ShellGenericContainerAllocation);
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (actor));

  st_theme_node_adjust_for_height (theme_node, &for_height);

  alloc->_refcount = 1;
  g_signal_emit (G_OBJECT (actor), shell_generic_container_signals[GET_PREFERRED_WIDTH], 0,
                 for_height, alloc);
  if (min_width_p)
    *min_width_p = alloc->min_size;
  if (natural_width_p)
    *natural_width_p = alloc->natural_size;
  shell_generic_container_allocation_unref (alloc);
}

static void
shell_generic_container_get_preferred_height (ClutterActor *actor,
                                              gfloat        for_width,
                                              gfloat       *min_height_p,
                                              gfloat       *natural_height_p)
{
  ShellGenericContainerAllocation *alloc = g_slice_new0 (ShellGenericContainerAllocation);
  StThemeNode *theme_node = st_widget_get_theme_node (ST_WIDGET (actor));

  st_theme_node_adjust_for_width (theme_node, &for_width);

  alloc->_refcount = 1;
  g_signal_emit (G_OBJECT (actor), shell_generic_container_signals[GET_PREFERRED_HEIGHT], 0,
                 for_width, alloc);
  if (min_height_p)
    *min_height_p = alloc->min_size;
  if (natural_height_p)
    *natural_height_p = alloc->natural_size;
  shell_generic_container_allocation_unref (alloc);
}

static void
shell_generic_container_paint (ClutterActor  *actor)
{
  ShellGenericContainer *self = (ShellGenericContainer*) actor;
  GList *iter;

  CLUTTER_ACTOR_CLASS (shell_generic_container_parent_class)->paint (actor);

  for (iter = self->priv->children; iter; iter = iter->next)
    {
      ClutterActor *child = iter->data;

      if (g_hash_table_lookup (self->priv->skip_paint, child))
        continue;

      clutter_actor_paint (child);
    }
}

static void
shell_generic_container_pick (ClutterActor        *actor,
                              const ClutterColor  *color)
{
  ShellGenericContainer *self = (ShellGenericContainer*) actor;
  GList *iter;

  CLUTTER_ACTOR_CLASS (shell_generic_container_parent_class)->pick (actor, color);

  for (iter = self->priv->children; iter; iter = iter->next)
    {
      ClutterActor *child = iter->data;

      if (g_hash_table_lookup (self->priv->skip_paint, child))
        continue;

      clutter_actor_paint (child);
    }
}

/**
 * shell_generic_container_get_n_skip_paint:
 * @container:  A #ShellGenericContainer
 *
 * Returns: Number of children which will not be painted.
 */
guint
shell_generic_container_get_n_skip_paint (ShellGenericContainer  *self)
{
  return g_hash_table_size (self->priv->skip_paint);
}

/**
 * shell_generic_container_set_skip_paint:
 * @container: A #ShellGenericContainer
 * @child: Child #ClutterActor
 * @skip %TRUE if we should skip painting
 *
 * Set whether or not we should skip painting @actor.  Workaround for
 * lack of gjs ability to override _paint vfunc.
 */
void
shell_generic_container_set_skip_paint (ShellGenericContainer  *self,
                                        ClutterActor           *child,
                                        gboolean                skip)
{
  gboolean currently_skipping;

  currently_skipping = g_hash_table_lookup (self->priv->skip_paint, child) != NULL;
  if (!!skip == currently_skipping)
    return;

  if (!skip)
    g_hash_table_remove (self->priv->skip_paint, child);
  else
    g_hash_table_insert (self->priv->skip_paint, child, child);
}

/**
 * shell_generic_container_remove_all:
 * @self: A #ShellGenericContainer
 *
 * Removes all child actors from @self.
 */
void
shell_generic_container_remove_all (ShellGenericContainer *self)
{
  /* copied from clutter_group_remove_all() */

  GList *children;

  children = self->priv->children;
  while (children)
    {
      ClutterActor *child = children->data;
      children = children->next;

      clutter_container_remove_actor (CLUTTER_CONTAINER (self), child);
    }
}

static void
shell_generic_container_finalize (GObject *object)
{
  ShellGenericContainer *self = (ShellGenericContainer*) object;

  g_hash_table_destroy (self->priv->skip_paint);

  G_OBJECT_CLASS (shell_generic_container_parent_class)->finalize (object);
}

static void
shell_generic_container_class_init (ShellGenericContainerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  gobject_class->finalize = shell_generic_container_finalize;

  actor_class->get_preferred_width = shell_generic_container_get_preferred_width;
  actor_class->get_preferred_height = shell_generic_container_get_preferred_height;
  actor_class->allocate = shell_generic_container_allocate;
  actor_class->paint = shell_generic_container_paint;
  actor_class->pick = shell_generic_container_pick;

  shell_generic_container_signals[GET_PREFERRED_WIDTH] =
    g_signal_new ("get-preferred-width",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  gi_cclosure_marshal_generic,
                  G_TYPE_NONE, 2, G_TYPE_FLOAT, SHELL_TYPE_GENERIC_CONTAINER_ALLOCATION);

  shell_generic_container_signals[GET_PREFERRED_HEIGHT] =
    g_signal_new ("get-preferred-height",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  gi_cclosure_marshal_generic,
                  G_TYPE_NONE, 2, G_TYPE_FLOAT, SHELL_TYPE_GENERIC_CONTAINER_ALLOCATION);

  shell_generic_container_signals[ALLOCATE] =
    g_signal_new ("allocate",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  gi_cclosure_marshal_generic,
                  G_TYPE_NONE, 2, CLUTTER_TYPE_ACTOR_BOX, CLUTTER_TYPE_ALLOCATION_FLAGS);

  g_type_class_add_private (gobject_class, sizeof (ShellGenericContainerPrivate));
}

static void
shell_generic_container_add_actor (ClutterContainer *container,
                                   ClutterActor     *actor)
{
  ShellGenericContainerPrivate *priv = SHELL_GENERIC_CONTAINER (container)->priv;

  _st_container_add_actor (container, actor, &priv->children);
}

static void
shell_generic_container_remove_actor (ClutterContainer *container,
                                      ClutterActor     *actor)
{
  ShellGenericContainerPrivate *priv = SHELL_GENERIC_CONTAINER (container)->priv;

  _st_container_remove_actor (container, actor, &priv->children);
}

static void
shell_generic_container_foreach (ClutterContainer *container,
                                 ClutterCallback   callback,
                                 gpointer          callback_data)
{
  ShellGenericContainerPrivate *priv = SHELL_GENERIC_CONTAINER (container)->priv;

  _st_container_foreach (container, callback, callback_data,
                         &priv->children);
}

static void
shell_generic_container_lower (ClutterContainer *container,
                               ClutterActor     *actor,
                               ClutterActor     *sibling)
{
  ShellGenericContainerPrivate *priv = SHELL_GENERIC_CONTAINER (container)->priv;

  _st_container_lower (container, actor, sibling, &priv->children);
}

static void
shell_generic_container_raise (ClutterContainer *container,
                        ClutterActor     *actor,
                        ClutterActor     *sibling)
{
  ShellGenericContainerPrivate *priv = SHELL_GENERIC_CONTAINER (container)->priv;

  _st_container_raise (container, actor, sibling, &priv->children);
}

static void
shell_generic_container_sort_depth_order (ClutterContainer *container)
{
  ShellGenericContainerPrivate *priv = SHELL_GENERIC_CONTAINER (container)->priv;

  _st_container_sort_depth_order (container, &priv->children);
}

static void
shell_generic_container_iface_init (ClutterContainerIface *iface)
{
  iface->add = shell_generic_container_add_actor;
  iface->remove = shell_generic_container_remove_actor;
  iface->foreach = shell_generic_container_foreach;
  iface->lower = shell_generic_container_lower;
  iface->raise = shell_generic_container_raise;
  iface->sort_depth_order = shell_generic_container_sort_depth_order;
}

static void
shell_generic_container_init (ShellGenericContainer *area)
{
  area->priv = G_TYPE_INSTANCE_GET_PRIVATE (area, SHELL_TYPE_GENERIC_CONTAINER,
                                            ShellGenericContainerPrivate);
  area->priv->skip_paint = g_hash_table_new (NULL, NULL);
}

GType
shell_generic_container_allocation_get_type (void)
{
  static GType gtype = G_TYPE_INVALID;
  if (gtype == G_TYPE_INVALID)
    {
      gtype = g_boxed_type_register_static ("ShellGenericContainerAllocation",
         (GBoxedCopyFunc)shell_generic_container_allocation_ref,
         (GBoxedFreeFunc)shell_generic_container_allocation_unref);
    }
  return gtype;
}
