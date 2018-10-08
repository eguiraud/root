#include <ROOT/RDataFrame.hxx>

template <typename DF>
ROOT::RDF::RInterface<ROOT::Detail::RDF::RJittedFilter> CustomTransformation(DF &df, bool c)
{
   // some transforms
   auto df2 = df.Filter([] { return true; }).Define("x", [] { return 42; });

   // conditional transform: must append a dummy jitted filter to normalize the return type of the lambda
   auto filter_if = [c, &df2] { return c ? df2.Filter([] { return true; }).Filter("true") : df2.Filter("true"); };

   return filter_if().Filter("true"); // another dummy filter to get an utterable return type
}

template <typename TDF>
TDF ApplyDefines(TDF df, const std::vector<std::string> &)
{
   return df;
}

template <typename TDF, typename DefFn, typename... Defines>
auto ApplyDefines(TDF df, const std::vector<std::string> &names, DefFn f, Defines... defines)
   -> decltype(ApplyDefines(df.Define(names[0], f), names, defines...))
{
   std::vector<std::string> new_names{names.begin() + 1, names.end()};
   return ApplyDefines(df.Define(names[0], f), new_names, defines...);
}

template <typename TDF, typename DefinedDF = decltype(std::declval<TDF>().Define("", ""))>
std::unique_ptr<DefinedDF>
ApplyDefines(TDF &df, const std::vector<std::string> &names, const std::vector<std::string> &exprs)
{
   auto latestDF = std::make_unique<DefinedDF>(df.Define(names[0], exprs[0]));

   for (auto i = 1u; i < names.size(); ++i)
      latestDF = std::make_unique<DefinedDF>(latestDF->Define(names[i], exprs[i]));

   return latestDF;
}

int main()
{
   /******** Adding a series of filters ********/

   /******** Adding a series of jitted filters ********/

   /******** Adding a series of defines *******/
   ROOT::RDataFrame d(0);
   auto dd = d.Filter([] { return true; });
   auto ddd = ApplyDefines(d, {"x", "y", "z"}, [] { return 1.; }, [] { return 2.; }, [] { return 3.; });

   /******** Adding a series of jitted defines *******/
   auto dddd = ApplyDefines(ddd, {"w", "t"}, {"double(rand())/RAND_MAX", "double(rand())/RAND_MAX*2"});

   /******** Performing a generic series of transformations ******/
   ROOT::RDataFrame df1(0);
   auto transformed = CustomTransformation(df1, true);

   /******** Performing a conditional transformation, according to a runtime-parameter ********/
   ROOT::RDataFrame df2(0);
   auto filter_if = [&df2](bool c) { return c ? df2.Filter([] { return true; }).Filter("true") : df2.Filter("true"); };
   auto filtered = filter_if(true);

   return 0;
}
