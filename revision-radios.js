
(function ($) {

  Drupal.behaviors.suppressPublishedForModeratedContent = {
    attach: function (context) {

      // Hide the "Published" check-box when "Create new revision and moderate" is pressed.
      var publishedBox = $('.form-item-status', context);
      if ($('.form-item-revision-operation .form-radio:checked').val() == 2) { // 2 == NEW_REVISION_WITH_MODERATION
        publishedBox.hide();
      }

      $('.form-radio').click(function() {
        if ($('.form-item-revision-operation .form-radio:checked').val() == 2) {
          publishedBox.hide();
          $('#edit-status', context).val(0);
        //@todo: update the summary tab to reflect publication status
        //var summaryText = $('fieldset.node-form-options', context).text();
        //summaryText = summaryText.replace('Published', 'Not published');
        }
        else {
          publishedBox.show();
        }
      });

    }
  };

})(jQuery);
