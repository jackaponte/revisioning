
(function ($) {

  Drupal.behaviors.suppressPublishedForModeratedContent = {
    attach: function (context) {

      // Hide the "Published" check-box when "Create new revision and moderate" is pressed.
      var publishedBox = $('.form-item-status', context);
      if ($('.form-item-revision-operation .form-radio:checked').val() == 2) { // 2 == NEW_REVISION_WITH_MODERATION
        $('#edit-status', context).val(0);
        publishedBox.hide();
      }

      $('.form-radio').click(function() {
        if ($('.form-item-revision-operation .form-radio:checked').val() == 2) {
          $('#edit-status', context).val(0);
          publishedBox.hide();
          var summaryTab = $('.vertical-tabs span.summary:last', context)
          var updatedText = summaryTab.text().replace('Published', 'Not published');
          summaryTab.text(updatedText);
        }
        else {
          publishedBox.show();
        }
      });

    }
  };

})(jQuery);
