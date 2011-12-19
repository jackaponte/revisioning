
(function ($) {

  Drupal.behaviors.showPublicationDateTimeField = {
    attach: function (context) {
      var publication_date = $('.form-item-publication-date');
      var new_revision_in_moderation_radio = $('#edit-revision-operation-2');

      if (!new_revision_in_moderation_radio.is(':checked')) {
        publication_date.hide();
      }
      $('#edit-revision-operation input').click(function() {
        if (new_revision_in_moderation_radio.is(':checked')) {
          publication_date.show();
        }
        else {
          publication_date.hide();
        }
      });
    }
  };

})(jQuery);

