<?php

/**
 * @file
 * Add tokens pertaining to the Revisioning module.
 *
 * For examples see the Token module, files token/token_<module>.inc
 *
 * Added to the set are the following, which are not provided by token_node.inc
 *   vid: Node revision ID
 *   revision-title: Revision title
 *   revision-author-uid: Revision author's user id
 *   revision-author-name: Revision author's user name
 *   revision-author-name-raw: Revision author's user name -- aw user input.
 *   revision-author-mail: Revision author's e-mail
 *   revision-author-mail-raw: Revision author's e-mail -- raw user input.
 *
 * @ingroup token
 */

/**
 * Implements hook_token_info().
 */
function revisioning_token_info() {
  // First specify token type(s)
  $info['types']['revision'] = array(
    'name' => t("Revisions"),
    'description' => t('Tokens related to revisions of individual content items, or "node revisions"'),
    'needs-data' => 'node',
  );
  // Then specify the tokens
  $info['tokens']['revision']['author'] = array(
    'name' => t("Author"),
    'description' => t("The author (editor) of the revision"),
    'type' => 'user'
  );
  $info['tokens']['revision']['body'] = array(
    'name' => t("Body"),
    'description' => t("The main body text of the revision")
  );
  $info['tokens']['revision']['created'] = array(
    'name' => t("Timestamp"),
    'description' => t("The date and time the revision was created."),
    'type' => 'date'
  );
  $info['tokens']['revision']['title'] = array(
    'name' => t("Title"),
    'description' => t("The title of the revision")
  );
  $info['tokens']['revision']['vid'] = array(
    'name' => t("Revision id"),
    'description' => t("The unique ID of the revision"),
  );
  return $info;
}

/**
 * Implements hook_tokens().
 */
function revisioning_tokens($type, $tokens, array $data = array(), array $options = array()) {
  $replacements = array();

  if ($type == 'revision' && !empty($data['revision'])) {
    $revision = $data['revision'];

    foreach ($tokens as $name => $original) {
      switch ($name) {
        case 'title':
          $title = $revision->title;
          $replacements[$original] = empty($options['sanitize']) ? $title : check_plain($title);
          break;
        case 'body':
          $text = $revision->body[$revision->language][0]['value'];
          $replacements[$original] = empty($options['sanitize']) ? $text : $check_plan($text);
          break;
        case 'vid':
          $replacements[$original] = $revision->vid;
          break;
      }
    }
    // Chained token relationships for revision author and revision timestamp
    if ($author_tokens = token_find_with_prefix($tokens, 'author')) {
      $author = user_load($revision->revision_uid);
      $replacements += token_generate('user', $author_tokens, array('user' => $author), $options);
    }
    if ($created_tokens = token_find_with_prefix($tokens, 'created')) {
      $replacements += token_generate('date', $created_tokens, array('date' => $revision->revision_timestamp), $options);
    }
  }
  return $replacements;
}

/**
 * Implements hook_token_values().
 *
function revisioning_token_values($type, $object = NULL, $options = array()) {
  $values = array();
  switch ($type) {
    case 'node':
      $values['vid'] = $object->vid;
      $values['revision-title'] = $object->title;
      _set_revision_author_values($object->revision_uid, $values);
      break;

    case 'op':
      switch ($object) {
        case 'publish':
        case 'revert':
        case 'unpublish':
          $nid = arg(1);
          $revision = node_load($nid, revisioning_get_current_node_revision_id($nid));
          $values['vid'] = $revision->vid;
          $values['revision-title'] = $revision->title;
          _set_revision_author_values($revision->revision_uid, $values);
      }
      break;
  }
  return $values;
}

function _set_revision_author_values($uid, &$values) {
  $result = db_query("SELECT name, mail FROM {users} WHERE uid = :uid", array(':uid' => $uid));
  $account = $result[0];
  $values['revision-author-uid'] = $uid;
  $values['revision-author-name'] = check_plain($account->name);
  $values['revision-author-name-raw'] = $account->name;
  $values['revision-author-mail'] = check_plain($account->mail);
  $values['revision-author-mail-raw'] = $account->mail;
}
 */