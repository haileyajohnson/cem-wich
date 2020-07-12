/**
* Controls tab view
*/
function ControlsTab() {
    return {
        $elem: $(".control-tab"),
        $tab: $("#control-tab"),

        $shelf_slope: $("input[name=shelf-slope]"),
        $shoreface_slope: $("input[name=shoreface-slope]"),
        $closure_depth: $("input[name=closure-depth]"),
        $sed_mobility: $("input[name=sed-mobility]"),
        $end_year: $("input[name=end-date]"),
        $length_timestep: $("input[name=timestep]"),
        $save_interval: $("input[name=save-interval]"),

        shelf_slope: .001,
        shoreface_slope: .01,
        closure_depth: 10.0,
        sed_mobility: 0.67,
        end_year: new Date().getFullYear() - 1,
        length_timestep: 1,
        save_interval: 365,
        num_timesteps: null,

        init: function() {
            this.$tab.click(() => { onTabChange(this); });
            // init values
            this.setAllValues();
            // attach listeners
            this.$shelf_slope.change(() => { this.onShelfSlopeChange(); });
            this.$shoreface_slope.change(() => { this.onShorefaceSlopeChange(); });
            this.$closure_depth.change(() => { this.onClosureDepthChange(); });
            this.$sed_mobility.change(() => { this.onSedMobilityChange(); });
            this.$end_year.change(() => { this.onEndYearChange(); });
            this.$length_timestep.change(() => { this.onTimestepLengthChange(); });
            this.$save_interval.change(() => { this.onSaveIntervalChange(); });
        },

        getNumTimesteps: function() {
            var startDate = new Date(gridTab.start_year + "-01-01");
            var endDate = new Date(this.end_year + "-12-31");
            var millis = endDate.getTime() - startDate.getTime();
            var days = millis / (1000 * 60 * 60 * 24);
            this.num_timesteps = Math.floor(days / this.length_timestep);
            if (runTab) { runTab.displayNumTimesteps(); }
        },

        /***********
         * listeners
         ***********/
        onShelfSlopeChange: function() {
            this.shelf_slope = this.$shelf_slope.val();
        },

        onShorefaceSlopeChange: function() {
            this.shoreface_slope = this.$shoreface_slope.val();
        },
        
        onClosureDepthChange: function() {
            this.closure_depth = this.$closure_depth.val();
        },

        onSedMobilityChange: function() {
            this.sed_mobility = this.$sed_mobility.val();
        },

        onEndYearChange: function() {
            this.end_year = this.$end_year.val();
            this.getNumTimesteps();
            runTab.displayTimestep(0);
        },

        onTimestepLengthChange: function() {
            this.length_timestep = this.$length_timestep.val();
            this.getNumTimesteps();
            runTab.displayTimestep(0);
        },

        onSaveIntervalChange: function() {
            this.save_interval = this.$save_interval.val();
        },

        /*********
         * setters
         *********/
        setAllValues: function() {
            this.getNumTimesteps();     
            this.$shelf_slope.val(this.shelf_slope);
            this.$shoreface_slope.val(this.shoreface_slope);
            this.$closure_depth.val(this.closure_depth);
            this.$sed_mobility.val(this.sed_mobility);
            this.$end_year.val(this.end_year);
            this.$length_timestep.val(this.length_timestep);
            this.$save_interval.val(this.save_interval);  
        }
    }
}