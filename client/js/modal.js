/**
 * Modal window
 */
function ModalInterface() {
    return {
        $modal: $(".modal"),
        $fillInput: $("input[name=fill]"),
        $cell: $(".cell"),

        feature: null,
        percentFull: 0,

        init: function() {
            // set up listeners
            this.$fillInput.change(() => {
                this.setFill();
                this.display();
            });
            this.$modal.find(".modal-ok-button").click(() => { this.onOkClicked(); } );
            this.$modal.find(".modal-cancel-button").click(() => { this.onCancelClicked() });
        },

        open: function(feature) {
            this.feature = feature;
            this.$fillInput.val(feature.get("fill")*100);
            this.setFill();
            this.display();
            this.$modal.show();
        },

        setFill: function() {
            this.percentFull = this.$fillInput.val();
        },

        display: function() {
            this.$cell.empty();
            var $cellFull = $("<div class='cell-full'></div>");
            var $cellEmpty = $("<div class='cell-empty'></div>");

            var styleFull = {
                "height": "" + this.percentFull + "%",
                "width": "100%"
            }
            var styleEmpty = {
                "height": "" + (100-this.percentFull) + "%",
                "width": "100%"
            }


            $cellEmpty.css(styleEmpty).css({"float":"left"}).appendTo(this.$cell);
            $cellFull.css(styleFull).css({"float":"right"}).appendTo(this.$cell);
        },

        save: function() {
            var i = this.feature.get('id');
            mapInterface.cemGrid[i] = this.percentFull/100;
            mapInterface.updateFeature(this.feature, this.percentFull/100);
        },

        close: function() {
            this.feature = null;
            this.$modal.hide();
        },

        /**
         * Listeners
         */        
        onOkClicked: function() {
            // save changes
            this.save();
            this.close();
        },

        onCancelClicked: function() {
            this.close();
        }
    };
}