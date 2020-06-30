/**
 * Run tab view
 */
function RunTab() {
    return {
        $elem: $(".run-tab"),
        $tab: $("#run-tab"),
        
        $output: $(".output-table > tbody"),
        $timestep: $(".timestep"),
        $runButton: $(".run-button"),
        $outputButton: $(".output-button"),

        init: function() {
            this.$tab.click(() => { onTabChange(this); });
            this.$runButton.click(() => { onRun(); });

            this.$runButton.disable();
            this.$outputButton.disable();

            this.displayTimestep(0);
            this.displayNumTimesteps();
        },

        /***********
         * callbacks
         ***********/
        updateOutput: function(msg, timestep) {
            var $trow = $("<tr></tr>");
            $trow.append($("<td></td>").text(timestep));

            var shape = msg.sp_pca;
            if (shape.hasOwnProperty("rotation")) {
                $trow.append($("<td></td>").text((shape.rotation).toFixed(3)));
            } else {
                $trow.append($("<td></td>").text("---"));
            }
            if (shape.hasOwnProperty("scale")) {
                $trow.append($("<td></td>").text((shape.scale).toFixed(3)));
            } else {
                $trow.append($("<td></td>").text("---"));
            }

            var S = msg.t_pca;
            if (S.length >= 3) {
                $trow.append($("<td></td>").text(S[0].toFixed(3)))
                .append($("<td></td>").text(S[1].toFixed(3)))
                .append($("<td></td>").text(S[2].toFixed(3)));
            } else {
                $trow.append($("<td></td>").text("---"))
                .append($("<td></td>").text("---"))
                .append($("<td></td>").text("---"));
            }

            $trow.appendTo(this.$output);
        },

        clearOutput: function() {
            this.$output.empty();
        },

        displayTimestep: function(t) {
            $(this.$timestep.find(".current-step")[0]).text("t = " + t);
        },

        displayNumTimesteps: function() {
            $(this.$timestep.find(".num-timesteps")[0]).text(controlTab.num_timesteps);
        }
    }
}