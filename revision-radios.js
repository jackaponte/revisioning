
(function ($) {

  Drupal.behaviors.suppressPublishedForModeratedContent = {
    attach: function (context) {

      // Hide the "Published" check-box when "Create new revision and moderate" is pressed.
      var publishedDiv = $('.form-item-status', context);

      if ($('.form-item-revision-operation .form-radio:checked').val() == 2) { // 2 == NEW_REVISION_WITH_MODERATION
        publishedDiv.hide(); // will already be unchecked, unless "Auto-publish" is on
      }

      $('.form-radio').click(function() {
        clicked = $('.form-item-revision-operation .form-radio:checked').val();

        if (clicked == 2) {
          $('.form-item-status input', context).removeAttr('checked');
          publishedDiv.hide();
          // @todo: don't do this when "Auto-publish" is ticked on content type (but how?)
          var summaryTab = $('.vertical-tabs span.summary:last', context);
          var updatedText = summaryTab.text().replace(Drupal.t('Published'), Drupal.t('Not published'));
          summaryTab.text(updatedText);
        }
        else {
          publishedDiv.show();
          if (clicked == 0) {
            var summaryTab = $('.vertical-tabs span.summary:first', context)
            var updatedText = summaryTab.text().replace(Drupal.t('New revision'), Drupal.t('No revision'));
            summaryTab.text(updatedText);
          }
        }

      });
    }
  };

})(jQuery);
