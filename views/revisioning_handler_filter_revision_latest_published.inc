<?php
/**
 * @file
 *   Views filter override to filter on revision recency, i.e. whether this is
 *   the latest published revision of a node or not.
 */
class revisioning_handler_filter_revision_latest_published extends views_handler_filter {

  function construct() {
    parent::construct();
  }

  function access() {
    return user_access('access content');
  }

  /**
   * Override the query, in particular the WHERE clause.
   */
  function query() {
    $revisions_table = $this->ensure_my_table();
    $node_table = $this->query->ensure_table('node');

    // The subquery selects the maximum revision ID (vid) for a given node ID
    // (nid). This is guaranteed to be the latest revision, because vids are
    // assigned chronologically. Status is used to select only revisions that
    // are published.
    $max_vid_subquery = "SELECT MAX(vid) FROM $revisions_table WHERE $revisions_table.nid = $node_table.nid AND $revisions_table.status=1";

    $where_clause = "$revisions_table.vid = ($max_vid_subquery)";
    $this->query->add_where_expression($this->options['group'], $where_clause);
  }
}
