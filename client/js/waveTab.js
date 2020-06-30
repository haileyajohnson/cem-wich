/**
 * Wave tab view
 */
function WaveTab() {
    return {
        $elem: $(".wave-tab"),
        $tab: $("#wave-tab"),

        $asymmetry: $("input[name=a-input]"),       
        $stability: $("input[name=u-input]"),
        $wave_height: $("input[name=wave-height]"),       
        $wave_period: $("input[name=wave-period]"),
        $upload_button: $(".upload-button"),

        a_val: 50,
        u_val: 50,
        wave_heights: [1.5],
        wave_periods: [10],
        wave_angles: [-1],

        init: function() {
            this.$tab.click(() => { onTabChange(this); });
            // init values
            this.setAllValues();
            // attach listeners
            this.$asymmetry.change(() => { this.onAsymmetryChange(); });
            this.$stability.change(() => { this.onStabilityChange(); });
            this.$wave_height.change(() => { this.onWaveHeightChange(); });
            this.$wave_period.change(() => { this.onWavePeriodChange(); });
            this.$upload_button.click(() => { $('#wave-file-input').trigger('click'); });
        },

        readWaveFile: function(csv) {
            try {
                var H = [];
                var T = [];
                var theta = [];
                var rows = csv.split('\n');
                for (var i = 0; i < rows.length; i++) {
                    var cols = rows[i].split(',');
                    if (cols[0]) {
                        H.push(parseFloat(cols[0]));
                        T.push(parseFloat(cols[1]));
                        theta.push(parseFloat(cols[2]));
                    }
                }
                this.wave_heights = H;
                this.wave_periods = T;
                this.wave_angles = theta;

                this.disableWaveInput();
                this.a_val = -1;
                this.u_val = -1;
            } catch (e)
            {
                return 1;
            }
        },

        /***********
         * listeners
         ***********/
        onAsymmetryChange: function() {
            this.a_val = this.$asymmetry.val();
            if (this.a_val >= 0) { 
                this.onWaveHeightChange();
                this.onWavePeriodChange();
                this.wave_angles = [-1];
            }
        },

        onStabilityChange: function() {
            this.u_val = this.$stability.val();
            if (this.u_val >= 0) {
                this.onWaveHeightChange();
                this.onWavePeriodChange();
                this.wave_angles = [-1];0
            }
        },

        onWaveHeightChange: function() {
            this.wave_heights = [parseFloat(this.$wave_height.val())];
        },

        onWavePeriodChange: function() {
            this.wave_periods = [parseFloat(this.$wave_period.val())];
        },

        /*********
         * setters
         *********/
        setAllValues: function() {
            if (this.a_val && this.u_val) {
                this.$asymmetry.val(this.a_val);
                this.$stability.val(this.u_val);
                this.$wave_height.val(this.wave_heights[0]);
                this.$wave_period.val(this.wave_periods[0]);
            }
            else {
                this.disableWaveInput();
            }
        },

        disableWaveInput: function() {
            this.$asymmetry.disable();
            this.$stability.disable();
            this.$wave_height.disable();
            this.$wave_period.disable();
        }
    }
}