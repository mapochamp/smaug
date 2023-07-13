% read in the ilp_stats.csv file
% create plots grouped by model
% x axis is size
% y axis is reduction factor or speedup
% maybe try to get them all on one plot as well (overlapping)

data = table2struct(readtable("ilp_stats_no_percent.csv"),"ToScalar",true);
model_names = unique(data.ModelName);

Y_reduction_factor = [];
Y_speedup = [];

for ii = 1:length(model_names)
    % get the data this model
    this_model_name = model_names{ii};
    ind = strcmp(data.ModelName,this_model_name);
    this_model_data = structfun(@(x) x(ind), data, "UniformOutput",false);

    reduction_factor = 100*this_model_data.ReductionFactor;
    speedup = 100*this_model_data.Speedup;
    Y_reduction_factor = [Y_reduction_factor; reduction_factor'];
    Y_speedup = [Y_speedup; speedup'];
    X = categorical(this_model_data.Size);
    X = reordercats(X, this_model_data.Size);

    % now create the reduction plot for this model
    r = figure; hold on;
    bar(X, reduction_factor, "DisplayName",this_model_name);
    title(["reduction factor of " this_model_name],"Interpreter","none");
    xlabel("size");
    ylabel("reduction factor (%)");
    legend("-DynamicLegend","Interpreter","none","Location","northwest");

    % create the speedup plot for this model
    s = figure; hold on;
    bar(X,speedup, "DisplayName",this_model_name);
    title(["speedup of " this_model_name],"Interpreter","none");
    xlabel("size");
    ylabel("speedup factor (%)");
    legend("-DynamicLegend","Interpreter","none","Location","northwest");

    % uncomment to autosave
    %savefig(r,strcat(this_model_name,"_reduction_plot.fig"));
    %savefig(s,strcat(this_model_name,"_speedup_plot.fig"));
end


% make a plot for all the models on the same one and grouped together
a = figure; hold on;
bar(X, Y_reduction_factor);
xlabel("scratchpad size");
ylabel("reduction factor (%)");
legend(model_names,"Interpreter","none");

b = figure; hold on;
bar(X, Y_speedup);
xlabel("scratchpad size");
ylabel("speedup factor (%)");
legend(model_names,"Interpreter","none");

%savefig(a,"all_models_reduction_plot.fig");
%savefig(b,"all_models_speedup_plot.fig");





 