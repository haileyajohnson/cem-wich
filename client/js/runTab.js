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
            // create row for current timestep
            var $trow = $("<tr></tr>");
            $trow.append($("<td></td>").text(timestep));

            // display similarity indices
            var S = msg.S;
            for (var k = 0; k < S.length; k++) {
                $trow.append($("<td></td>").text(S[k].toFixed(3)));
            }
            for (var k = S.length; k < 5; k++) {
                $trow.append($("<td></td>").text("---"));
            }

            // display percent explained variability
            var wave_var = msg.w < 0 ? "---" : (msg.w*100).toFixed(3)     
            $trow.append($("<td></td>").text(wave_var));
            
            // appeand row to output table
            $trow.appendTo(this.$output);
        },

        clearOutput: function() {
            this.$output.empty();
            this.displayTimestep(0);
        },

        displayTimestep: function(t) {
            $(this.$timestep.find(".current-step")[0]).text("t = " + t);
        },

        displayNumTimesteps: function() {
            $(this.$timestep.find(".num-timesteps")[0]).text(controlTab.num_timesteps);
        }
    }
}