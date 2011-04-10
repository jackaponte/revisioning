<?php

/**
 * @file
 * API functions of Revisioning module
 *
 * Reusable functions that do the dirty work.
 */

define('REVISION_ARCHIVED', 0);
define('REVISION_CURRENT', 1);
define('REVISION_PENDING', 2);

/**
 * Some naming conventions
 *
 * Pending:
 *   - revision with (vid > current_vid) of ANY node
 *     OR single revision of UNPUBLISHED node
 * Current, published:
 *   - revision with (vid == current_vid) of PUBLISHED node
 * Archived:
 *   - all other revisions, i.e.
 *     revision with (vid < current_vid) of ANY node
 *     OR revision with (vid == current_vid) of UNPUBLISHED node
 *
 * Note: these will change when Revisioning is going to store revision states
 * independently from vid number (e.g. in different table).
 */

/**
 * We use this mainly to set up some useful node properties that can be read
 * later, whether it be in this module or another, thus removing the need for
 * multiple calls to retrieve the same info in various places.
 * Called from revisioning_nodeapi();
 */
function revisioning_set_basic_node_revision_info(&$node) {
  if (!isset($node->current_revision_id)) {
    $node->current_revision_id = revisioning_get_current_node_revision_id($node->nid);
    $node->is_current = revisioning_revision_is_current($node);
    $node->num_revisions = revisioning_get_number_of_revisions($node->nid);
  }
  //$node->uid and $node->revision_uid were already set in node_load()
  //$node->revision is set as part of 'prepare'-op, see node_object_prepare()
}

/**
 * Get the number of revisions belonging to a node.
 * @param
 *  $nid, id of the node
 * @return
 *  A count representing the number of revisions associated with the node
 */
function revisioning_get_number_of_revisions($nid) {
  $result = db_query("SELECT COUNT(vid) FROM {node_revision} WHERE nid = :nid", array(':nid' => $nid));
  return $result->fetchField();
}

/**
 * Get the id of the current revision that the supplied node is pointing to.
 * Used in cases where the node object wasn't fully loaded or was loaded
 * with a different revision.
 *
 * @param $nid
 *  The id of the node whose current revision id is to be returned.
 * @return
 *  A single number being the current revision id (vid).
 */
function revisioning_get_current_node_revision_id($nid) {
  $result = db_query("SELECT vid FROM {node} WHERE nid = :nid", array(':nid' => $nid));
  return $result->fetchField();
}

/**
 * Get the id of the user who last edited the supplied node, ie. the author
 * of the latest revision.
 * This is irrespective of whether this latest revision is pending or not,
 * unless TRUE is specified for the second argument, in which case the uid
 * of the creator of the current revision (published or not) is returned.
 *
 * @param $nid
 *  The id of the node whose most recent editor id is to be returned.
 * @param $current
 *  Whether the uid of the current or very latest revision should be returned.
 * @return
 *  A single number being the user id (uid).
 */
function revisioning_get_last_editor($nid, $current = FALSE) {
  $sql = ($current) 
    ? "SELECT vid FROM {node} WHERE nid = :nid"
    : "SELECT MAX(vid) FROM {node_revision} WHERE nid = :nid";
  $vid = db_query($sql, array(':nid' => $nid))->fetchField();
  $result = db_query("SELECT uid FROM {node_revision} WHERE vid = :vid", array(':vid' => $vid));
  return $result->fetchField();
}

/**
 * Return whether the currenly loaded revision is the current one.
 * @param $node
 * @return
 *  TRUE if the currently loaded node revision is the current revision
 */
function revisioning_revision_is_current($node) {
  return !is_null($node) && ($node->current_revision_id == $node->vid);
}

/**
 * Return whether the supplied content type is subject to moderation.
 *
 * @param $content_type
 *  i.e. machine name, ie. $node->type
 * @return
 *  TRUE, if the supplied type has the "New revision in draft, pending
 *  moderation" box ticked on the Structure>>Content types>>edit page
 *  (see Revisioning).
 */
function revisioning_content_is_moderated($content_type) {
  return !empty($content_type) && in_array('revision_moderation', variable_get('node_options_' . $content_type, array()));
}


/**
 * Return a single or all possible revision state names.
 *
 * @param $state
 *  optional state id, as defined in revisioning_api.inc
 * @return
 *  if $state is provided, state name. Otherwise, state names array keyed by state id.
 */
function revisioning_revision_states($state = NULL) {
  static $states;
  $states = array(
    REVISION_ARCHIVED => t('Archived'),
    REVISION_CURRENT  => t('Current, published'),
    REVISION_PENDING  => t('Pending'),
  );
  return $state === NULL ? $states : $states[$state];
}

/**
 * Return TRUE when either of the following is true:
 * o the supplied node has at least one revision more recent than the current
 * o the node is not yet published and consists of a single revision
 *
 * Relies on vid, current_revision_id and num_revisions set on the node object,
 * see function revisioning_set_basic_node_revision_info()
 *
 * @param $node
 * @return TRUE, if node is pending according to the above definition
 */
function _revisioning_node_is_pending($node) {
  return ($node->vid > $node->current_revision_id) || (!$node->status && $node->num_revisions == 1);
}

/**
 * Implements hook_revisionapi().
 *
 * Act on various revision events.
 *
 * @param $op
 *  Operation
 * @param $node
 *  Node of current operation (loaded with vid of the operation).
 *
 * "Pre" operations can be useful to get values before they are lost or changed,
 * for example, to save a backup of revision before it's deleted.
 * Also, for "pre" operations vetoing mechanics could be implemented, so it
 * would be possible to veto an operation via hook_revisionapi(). For example,
 * when the hook is returning FALSE, operation will be vetoed.
 */
function revisioning_revisionapi($op, $node) {
  switch ($op) {

    case 'pre publish':
      if (module_exists('rules')) {
        rules_invoke_event('revisioning_pre_publish', $node);
      }
      break;

    case 'post publish': // called from _revisioning_publish_revision
      // Invoke hook_revision_publish() triggers passing the node as an arg
      module_invoke_all('revision_publish', $node);

      if (module_exists('rules')) {
        rules_invoke_event('revisioning_post_publish', $node);
      }
      break;

  //case 'pre unpublish':
      // Not implemented: do we really need it ?

    case 'post unpublish': // called from _revisioning_unpublish_revision()
      // Invoke hook_revision_unpublish triggers passing the node as an arg
      module_invoke_all('revision_unpublish', $node);

      if (module_exists('rules')) {
        rules_invoke_event('revisioning_post_unpublish', $node);
      }
      break;

    case 'pre revert':
      if (module_exists('rules')) {
        rules_invoke_event('revisioning_pre_revert', $node);
      }
      break;

    case 'post revert': // called from revisioning_revert_confirm_post_submit()
      // Invoke hook_revision_revert() triggers passing the node as an arg
      module_invoke_all('revision_revert', $node);

      if (module_exists('rules')) {
        rules_invoke_event('revisioning_post_revert', $node);
      }
      break;

    case 'pre delete':
      if (module_exists('rules')) {
        rules_invoke_event('revisioning_pre_delete', $node);
      }
      break;

    case 'post delete':
      break;
  }
}

/**
 * Get the id of the latest revision belonging to a node.
 * @param
 *  $nid, id of the node
 * @return
 *  ID of the latest revision.
 */
function revisioning_get_latest_revision_id($nid) {
  $result = db_query("SELECT MAX(vid) FROM {node_revision} WHERE nid = :nid", array(':nid' => $nid));
  return $result->fetchField();
}

/**
 * Revert node to selected revision without changing its publication status.
 *
 * @param $node
 *  Target $node object (loaded with target revision) or nid of target node
 * @param $vid
 *  Optional vid of revision to revert to, if provided $node must not be an object.
 */
function _revisioning_revertpublish_revision(&$node, $vid = NULL) {
  $node_revision = is_object($node) ? $node : node_load($node, $vid);
  module_invoke_all('revisionapi', 'pre revert', $node_revision);
  _revisioning_revert_revision($node_revision);
  module_invoke_all('revisionapi', 'post revert', $node_revision);
}

/**
 * Revert node to selected revision without publishing it.
 *
 * This is same as node_revision_revert_confirm_submit() in node_pages.inc,
 * except it doesn't put any messages on screen.
 *
 * @param $node
 *  Target $node object (loaded with target revision) or nid of target node
 * @param $vid
 *  optional vid of revision to revert to, if provided $node is not an object.
 */
function _revisioning_revert_revision(&$node, $vid = NULL) {
  $node_revision = is_object($node) ? $node : node_load($node, $vid);
  $node_revision->revision = 1;
  $node_revision->log = t('Copy of the revision from %date.', array('%date' => format_date($node_revision->revision_timestamp)));
  if (module_exists('taxonomy')) {
    $node_revision->taxonomy = array_keys($node_revision->taxonomy);
  }
  node_save($node_revision);
  watchdog('content', '@type: reverted %title revision %revision.', array('@type' => $node_revision->type, '%title' => $node_revision->title, '%revision' => $node_revision->vid));
}

/**
 * Publish node, without calling node_save()
 *
 * @param $node
 *  Target $node object or nid of target node
 */
function _revisioning_publish_node($nid_or_node) {
  $node = is_object($nid_or_node) ? $nid_or_node : node_load($nid_or_node);
  db_query("UPDATE {node} SET status = 1 WHERE nid = :nid", array(
    ':nid' => $node->nid)
  );
  db_query("UPDATE {node_revision} SET status = 1 WHERE vid = :vid", array(
    ':vid' => $node->vid)
  );
  // Database integrity: make sure that no other node revision has its published flag set
  db_query("UPDATE {node_revision} SET status = 0 WHERE nid = :nid AND vid <> :vid", array(
    ':nid' => $node->nid,
    ':vid' => $node->vid)
  );
  cache_clear_all();
}

/**
 * Unpublish node, without calling node_save().
 *
 * @param $node
 *  Target $node object or nid of target node
 */
function _revisioning_unpublish_node($nid_or_node) {
  $nid = is_object($nid_or_node) ? $nid_or_node->nid : $nid_or_node;
  db_query("UPDATE {node} SET status = 0 WHERE nid = :nid", array(
    ':nid' => $nid)
  );
  // Database integrity: make sure that no node revisions have their published flag set
  db_query("UPDATE {node_revision} SET status = 0 WHERE nid = :nid", array(
    ':nid' => $nid)
  );
  cache_clear_all();
}

/**
 * Delete selected revision of node, provided it's not current.
 *
 * This is same as node_revision_delete_confirm_submit() in node_pages.inc,
 * except it doesn't put any messages on the screen. This way it becomes
 * reusable (eg. in actions).
 * Since we are calling nodeapi as in node_revision_delete_confirm_submit(), we
 * invoke our "post delete" revisionapi hook in nodeapi. This way revisionapi
 * hooks work the same way both with "delete revision" submit handler and when
 * this function is called, and we don't invoke revisionapi "post delete" hook
 * twice.
 *
 * @param $node
 *  Target $node object (loaded with target revision) or nid of target node
 * @param $vid
 *  optional vid of revision to delete, if provided $node is not object.
 *
 * @TODO: Insert check to prevent deletion of current revision of node.
 */
function _revisioning_delete_revision(&$node, $vid = NULL) {
  $node_revision = is_object($node) ? $node : node_load($node, $vid);
  module_invoke_all('revisionapi', 'pre delete', $node_revision);
  db_query("DELETE FROM {node_revision} WHERE vid = :vid", array(
    ':vid' => $node_revision->vid)
  );
  module_invoke_all('node_' . $node_revision, 'delete revision');
  watchdog('content', '@type: deleted %title revision %revision.', array('@type' => $node_revision->type, '%title' => $node_revision->title, '%revision' => $node_revision->vid));
}

/**
 * Unpublish revision (i.e. the node).
 *
 * Note that no check is made as to whether the initiating user has permission
 * to unpublish this node.
 *
 * @param $node
 *  Target $node object or nid of target node
 *
 * @TODO: Shouldn't we invoke hook_nodeapi('update'..) too, since we are updating node?
 */
function _revisioning_unpublish_revision(&$node) {
  $node_revision = is_object($node) ? $node : node_load($node);
  module_invoke_all('revisionapi', 'pre unpublish', $node_revision);
  _revisioning_unpublish_node($node_revision->nid);
  watchdog('content', 'Unpublished @type %title', array('@type' => $node_revision->type, '%title' => $node_revision->title), WATCHDOG_NOTICE, l(t('view'), "node/$node_revision->nid"));
  module_invoke_all('revisionapi', 'post unpublish', $node_revision);
}

/**
 * Make the supplied revision of the node current and publish it.
 * It is the caller's responsibility to provide a proper revision.
 * Note that no check is made as to whether the initiating user has permission
 * to publish this revision.
 *
 * @param $node
 *  Target $node object (loaded with target revision) or nid of target node
 * @param $vid
 *  optional vid of revision to make current, if provided $node is not object.
 */
function _revisioning_publish_revision(&$node, $vid = NULL) {
  $node_revision = is_object($node) ? $node : node_load($node, $vid);
  module_invoke_all('revisionapi', 'pre publish', $node_revision);
  // Update node table, making sure the "published" (ie. status) flag is set
  db_query("UPDATE {node} SET vid = :vid, title = :title, status = 1 WHERE nid = :nid", array(
    ':vid' => $node_revision->vid,
    ':title' => $node_revision->title,
    ':nid' => $node_revision->nid)
  );
  db_query("UPDATE {node_revision} SET status = 1 WHERE vid = :vid", array(
    ':vid' => $node_revision->vid)
  );
  // Database integrity: make sure that no other node revision has its published flag set
  db_query("UPDATE {node_revision} SET status = 0 WHERE nid = :nid AND vid <> :vid", array(
    ':nid' => $node_revision->nid,
    ':vid' => $node_revision->vid)
  );
  cache_clear_all();
  watchdog('content', 'Published rev #%revision of @type %title', array('@type' => $node_revision->type, '%title' => $node_revision->title, '%revision' => $node_revision->vid), WATCHDOG_NOTICE, l(t('view'), "node/$node_revision->nid/revisions/$node_revision->vid/view"));
  module_invoke_all('revisionapi', 'post publish', $node_revision);
}

/**
 * Find the most recent pending revision, make it current, unless it already is
 * and publish node.
 * Note that no check is made as to whether the initiating user has permission
 * to publish this node.
 *
 * @param $node
 *   The node object whose latest pending revision is to be published
 * @return
 *   TRUE if operation was successful, FALSE if there is no pending revision to
 *   publish
 */
function _revisioning_publish_latest_revision(&$node) {
  // Get the latest pending revision
  $pending_revisions = _revisioning_get_pending_revisions($node->nid);
  $latest_pending = array_shift($pending_revisions);
  // If there is no pending revision, take the current revision, provided it's UNpublished
  if (!$latest_pending && $node->is_current && !$node->status) {
    _revisioning_publish_revision($node);
    return TRUE;
  }
  if ($latest_pending) {
    _revisioning_publish_revision($node->nid, $latest_pending->vid);
    return TRUE;
  }
  return FALSE;
}

/**
 * Return a count of the number of revisions newer than the supplied vid.
 *
 * @param $vid
 *  The reference vid.
 * @param $nid
 *  The id of the node.
 * @return
 *  integer
 */
function _revisioning_get_number_of_revisions_newer_than($vid, $nid) {
  $result = db_query("SELECT COUNT(vid) FROM {node_revision} WHERE nid = :nid AND vid > :vid",
    array(':vid' => $vid,
          ':nid' => $nid)
  );
  return $result->fetchField();
}

/**
 * Return a count of the number of revisions newer than the current revision.
 *
 * @param $nid
 *  The id of the node.
 * @return
 *  integer
 */
function _revisioning_get_number_of_pending_revisions($nid) {
  $result = db_query("SELECT COUNT(*) FROM {node} n INNER JOIN {node_revision} r ON n.nid = r.nid WHERE (r.vid > n.vid AND n.nid = :nid)", array(
    ':nid' => $nid)
  );
  return $result->fetchField();
}

/**
 * Retrieve a list of revisions with a vid greater than the current.
 *
 * @param $nid
 *  The node id to retrieve.
 * @return
 *  An array of revisions (latest first), each containing vid, title and
 *  content type.
 */
function _revisioning_get_pending_revisions($nid) {
  $sql = "SELECT r.vid, r.title, n.type FROM {node} n INNER JOIN {node_revision} r ON n.nid = r.nid WHERE (r.vid > n.vid AND n.nid = :nid) ORDER BY r.vid DESC";
  $result = db_query($sql, array(
    ':nid' => $nid)
  );
  $revisions = array();
  foreach ($result as $revision) {
    $revisions[$revision->vid] = $revision;
  }
  return $revisions;
}

/**
 * Retrieve a list of all revisions (archive, current, pending) belonging to
 * the supplied node.
 *
 * @param $nid
 *  The node id to retrieve.
 * @param $include_taxonomy_terms
 *  Whether to also retrieve the taxonomy terms for each revision
 * @return
 *  An array of revision objects, each with published flag, log message, vid,
 *  title, timestamp and name of user that created the revision
 */
function _revisioning_get_all_revisions_for_node($nid, $include_taxonomy_terms = FALSE) {
  $sql_select = 'SELECT n.vid AS current, r.vid, r.status, r.title, r.log, r.uid, r.timestamp, u.name';
  $sql_from   = ' FROM {node_revision} r INNER JOIN {node} n ON r.nid=n.nid INNER JOIN {users} u ON r.uid=u.uid';
  $sql_where  = ' WHERE r.nid = :nid ORDER BY r.vid DESC';
  if ($include_taxonomy_terms) {
    $sql_select .= ', ttd.name AS term';
    $sql_from .= ' LEFT JOIN {field_revision_field_tags} frt ON r.vid = frt.revision_id LEFT JOIN {taxonomy_term_data} ttd ON frt.field_tags_tid=ttd.tid';
    $sql_where .= ', term ASC';
  }
  $sql = $sql_select . $sql_from . $sql_where;
  $result = db_query($sql, array(':nid' => $nid));
  $revisions = array();
  foreach ($result as $revision) {
    if (empty($revisions[$revision->vid])) {
      $revisions[$revision->vid] = $revision;
    }
    elseif ($include_taxonomy_terms) {
      // If a revision has more than one taxonomy term, these will be returned
      // by the query as seperate objects differing only in their terms.
      $revisions[$revision->vid]->term .= ' ' . $revision->term;
    }
  }
  return $revisions;
}

/**
 * Return revision type of the supplied node.
 *
 * @param &$node
 *  Node object to check
 * @return
 *  Revision type
 */
function _revisioning_revision_is(&$node) {
  if ($node->is_pending) {
    return REVISION_PENDING;
  }
  return ($node->is_current && $node->status) ? REVISION_CURRENT : REVISION_ARCHIVED;
}

/**
 * Return a string with details about the node that is about to be displayed.
 *
 * @param $node
 *  The node that is about to be viewed
 * @return
 *  A translatable message containing details about the node
 */
function _revisioning_node_info_msg($node) {
  // Get username for the revision, not the creator of the node
  $revision_author = user_load($node->revision_uid);
  $placeholder_data = array(
    '@content_type' => $node->type,
    '%title' => $node->title,
    '!author' => theme('username', array('account' => $revision_author)),
    '@date' => format_date($node->revision_timestamp, 'small'),
  );
  $revision_type = _revisioning_revision_is($node);
  switch ($revision_type) {
    case REVISION_PENDING:
      return t('Displaying <em>pending</em> revision of @content_type %title, last modified by !author on @date', $placeholder_data);

    case REVISION_CURRENT:
      return t('Displaying <em>current, published</em> revision of @content_type %title, last modified by !author on @date', $placeholder_data);

    case REVISION_ARCHIVED:
      return t('Displaying <em>archived</em> revision of @content_type %title, last modified by !author on @date', $placeholder_data);
  }
}

/**
 * Return TRUE only if the user account has ALL of the supplied permissions.
 *
 * @param $permissions
 *  An array of permissions (strings)
 * @param $account
 *  The user account object. Defaults to the current user if omitted.
 * @return bool
 */
function revisioning_user_access_all($permissions, $account = NULL) {
  foreach ($permissions as $permission) {
    if (!user_access($permission, $account)) {
      return FALSE;
    }
  }
  return TRUE;
}

/**
 * Return an array of names of content types that are subject to moderation.
 *
 * @return array of strings, may be empty
 */
function revisioning_moderated_content_types($machine_name = TRUE) {
  $moderated_content_types = array();
  foreach (node_type_get_types() as $type) {
    $content_type = check_plain($type->type);
    if (revisioning_content_is_moderated($content_type)) {
      $moderated_content_types[] = ($machine_name ? $content_type : $type->name);
    }
  }
  return $moderated_content_types;
}


define('NO_FILTER', '-1');

/**
 * Retrieve a list of nodes or revisions accessible to the logged-in user via
 * the supplied operation.
 *
 * @param $op
 *   Operation, one of 'view', 'update' or 'delete'
 * @param $is_published
 *   1 to return only published content
 *   0 to return only content that isn't published
 *  -1 (default) no filter, return content regardles of publication status
 * @param $creator_uid
 *   Only return content created by the user with the supplied id.
 *   Defaults to -1, which means don't care who the creator is.
 * @param $modifier_uid
 *   Only return content last modified by the user with the supplied id.
 *   Defaults to -1, which means don't care who last modifed the node.
 * @param $is_moderated
 *   TRUE to return only content of types that are subject to moderation
 *   FALSE to return only content that isn't subject to moderation
 *   -1 (default) no filter, return content regardles of moderation flag
 * @param $is_pending
 *   Boolean indicating whether only nodes pending publication should be
 *   returned; a pending node is defined as a node that has a revision newer
 *   than the current OR a node with a single revision that is not published.
 * @param $max
 *   Maximum number of nodes to be returned, defaults to 1000
 * @param $order_by_override
 *   "ORDER BY ..." clause to be added, defaults to "timestamp DESC".
 * @return
 *   An array of node objects each containing nid, content type, published flag,
 *   creator-id, title+vid+modifier-id+timestamp of the current revision, plus taxonomy
 *   term(s) and workflow state, if these modules are installed and enabled.
 *
 * @todo
 *   Allow paging, improve performance
 */
function revisioning_get_nodes($op, $is_published = -1, $creator_uid = -1, $modifier_uid = -1,
  $is_moderated = -1, $is_pending = FALSE, $max = 1000, $order_by_override = NULL) {
  $sql_select = 'SELECT n.nid, r.vid, n.uid AS creator_uid, r.uid, n.type, n.status, r.title, r.timestamp';
  // Join on current revision (vid) except when looking for pending revisions
  $sql_from   = ' FROM {node} n INNER JOIN {node_revision} r ' . ($is_pending ? 'ON n.nid=r.nid' : 'ON n.vid=r.vid');
  $sql_where  = ($is_published < 0) ? '' : " WHERE n.status=$is_published";
  if ($creator_uid >= 0) {
    $sql_where  = empty($sql_where) ? " WHERE n.uid=$creator_uid" : $sql_where . " AND n.uid=$creator_uid";
  }
  if ($modifier_uid >= 0) {
    $sql_where  = empty($sql_where) ? " WHERE r.uid=$modifier_uid" : $sql_where . " AND r.uid=$modifier_uid";
  }
  if ($is_pending) {
    $sql_where  = empty($sql_where) ? ' WHERE' : $sql_where . ' AND';
    $sql_where .= ' (r.vid>n.vid OR (n.status=0 AND (SELECT COUNT(vid) FROM {node_revision} WHERE nid=n.nid)=1))';
  }
  $sql_order = " ORDER BY " . (empty($order_by_override) ? _revisioning_extract_order_clause_from_URI() : $order_by_override);
  $include_taxonomy_terms = module_exists('taxonomy') &&
    variable_get('revisioning_show_taxonomy_terms', TRUE) && (count(taxonomy_get_vocabularies()) > 0);
  $include_workflow_state = module_exists('workflow') && count(workflow_get_all()) > 0;
  if ($include_taxonomy_terms) {
    $sql_select .= ', ttd.name AS term';
    $sql_from .= ' LEFT JOIN {field_revision_field_tags} frt ON r.vid = frt.revision_id LEFT JOIN {taxonomy_term_data} ttd ON frt.field_tags_tid=ttd.tid';
  }
  if ($include_workflow_state) {
    $sql_select .= ', ws.state';
    $sql_from .= ' LEFT JOIN {workflow_node} wn ON wn.nid=n.nid LEFT JOIN {workflow_states} ws ON wn.sid=ws.sid';
  }
  $sql = $sql_select . $sql_from . $sql_where . $sql_order;
  $node_query_result = db_query_range($sql, 0, $max);
  $nodes = array();
  foreach ($node_query_result as $node) {
    //@todo: express revisioning_content_is_moderated() as WHERE clause, ie. let SQL do the filtering a priori
    //See revisioning_handler_filter_node_revision_moderation for WHERE clause example.
    $filter = ($is_moderated < 0) || $is_moderated == revisioning_content_is_moderated($node->type);
    $access = module_exists('module_grants') ? module_grants_node_access($op, $node) : node_access($op, $node);
    if ($filter && $access) {
      if (empty($nodes[$node->nid])) {
        $nodes[$node->nid] = $node;
      }
      // If a node has more than one taxonomy term, these will be returned by
      // the query as seperate objects differing only in their terms.
      elseif ($include_taxonomy_terms && !empty($node->term)) {
        // When $is_pending==TRUE more dan one revision may be returned, so
        // need to check for duplicate terms.
        $existing_node = $nodes[$node->nid];
        if (strpos($existing_node->term, $node->term) === FALSE) {
          // Bit of a quick & dirty -- goes wrong if a term is substr of another
          $existing_node->term .= " $node->term";
        }
      }
    }
  }
  return $nodes;
}

/**
 * Extract from the incoming URI (as in the table column header href)
 * the sort field and order for use in an SQL 'ORDER BY' clause.
 * @param
 *   none
 * @return
 *   db table field name and sort direction as a string
 */
function _revisioning_extract_order_clause_from_URI() {
  // We shouldn't have to do this, as tablesort.inc/tablesort_header(), called
  // from theme_table() is meant to look after it, but it's got a bug [#480382].
  // Note: this function is secure, as we're only allowing recognised values,
  //       all unknown values, result in a descending sort by 'timestamp'.
  switch ($order_by = drupal_strtolower($_REQUEST['order'])) {
    case 'creator':
      $order_by = 'n.uid';
      break;
    case 'by':
      $order_by = 'r.uid';
      break;
    case 'published?':
      $order_by = 'status';
      break;
    case 'workflow state':
      $order_by = 'state';
      break;
    // List names that are fine the way they are here:
    case 'title':
    case 'type':
    case 'term':
      break;
    default:
      $order_by = 'timestamp';
      break;
  }
  $direction = (drupal_strtolower($_REQUEST['sort'])== 'asc') ? 'ASC' : 'DESC';
  return "$order_by $direction";
}
