#include "def_grid.hxx"

//https://arne-mertz.de/2018/05/overload-build-a-variant-visitor-on-the-fly/
//https://medium.com/@nerudaj/std-visit-is-awesome-heres-why-f183f6437932


grid_cfg::grid_cfg (u8 nd, py::dict cfg) {
	u8 md=1; for (auto i{0}; i<nd; ++i) md*=3;
	
	if (py::len(cfg["step"]) != nd) throw std::invalid_argument("step");
	for (auto ds : cfg["step"]) {
		step.push_back(py::cast<f32>(ds));
	}
	
	// setup axes
	if (py::len(cfg["axes"]) != nd) throw std::invalid_argument("axes");
	size_t lctr_size{1};
	for (auto axis : cfg["axes"]) {
		std::vector<u32> axpts;
		std::vector<f32>    edpts;
		for (auto pt : axis) {
			axpts.push_back(py::cast<u32>(pt));
			edpts.push_back(py::cast<f32   >(pt) * step[shape.size()]);
		}
		lctr_size *= axpts.size()+1;
		shape.push_back(axpts.size()-1);
		units.push_back(axpts[axpts.size()-1]);
		axes .push_back(std::move(axpts));
		edges.push_back(std::move(edpts));
	}
	lctr.resize(lctr_size, 0);
	
	// setup flags
	flags.insert({"cylcrd", 0}); if (cfg.contains("cylcrd")) {
		if (nd == 2 and py::cast<std::string>(cfg["cylcrd"]) == "x"s) {
			flags["cylcrd"] = 1;
		} else {
			throw std::invalid_argument
			(fmt::format("grid{} isn't suitable for cylindrical coordinates!", nd));
		}
	}
	flags.insert({"loopax", 0}); if (cfg.contains("loopax")) {
		for (auto c : py::cast<std::string>(cfg["loopax"])) {
			if (u8(c-'x') >= nd) {
				throw std::invalid_argument
				(fmt::format("invalid loop axis: \'{}\'!", c));
			}
			flags["loopax"] |= 1<<(c-'x'); //TODO CHECK FİX
		}
	}
	
	// setup nodes
	for (auto node : cfg["nodes"]) {
		std::vector<u32> map; map.reserve(nd);
		std::vector<u32> lnk; lnk.reserve(md);
		std::vector<u8 > mcache;
		
		/*
		for (auto x : node["map"]) {
			map.push_back(py::cast<u32>(x));
		}
		*/
		for (auto m : node) {
			map.push_back(py::cast<u32>(m));
		}
		
		
		size_t k{0}, sh{1};
		for (int i{nd-1}; i >= 0; --i) {
			k  += (map[i]+1)*sh;
			sh *= shape[i]+2;
		}
		lctr[k] = nodes.size()+1;
		
		u64 mshift{0};
		if (cfg.contains("mask") and not cfg["mask"].is_none()) {
			auto info = (py::cast<py::array_t<u8, py::array::c_style>>(cfg["mask"])).request();

			auto fn = [&] (auto && fn, ssize_t n=0, ssize_t shift=0) -> void {
				if (n == nd) {
					u8 var{static_cast<u8*>(info.ptr)[shift]};
					mcache.push_back(var);
				} else for (size_t j1{axes[n][map[n]]}, j2{axes[n][1+map[n]]}; j1<j2; ++j1) {
					fn(fn, n+1, shift + j1*info.strides[n]/info.itemsize);
				}
			}; fn(fn);
			if (std::any_of(mcache.begin(), mcache.end(), [] (u8 m) {return m>0;})) {
				mshift = 1+mask.size();
				std::copy(mcache.begin(), mcache.end(), std::back_inserter(mask));
			}
		}
		nodes.push_back({std::move(map), std::move(lnk), mshift});
	}
	
	// build links
	for (auto& node : nodes) {
		auto &&fn = [&, k=0] (auto &&fn, u8 n, u32 sh=0, u32 w=1) mutable -> void {
			if (n>0) for (u32 i{1}, m; i<=3; ++i) {
				m=node.map[n-1]+i%3;
				if (CHECK_BIT(flags["loopax"], n-1) and m%(1+shape[n-1]) == 0) {
					m = m!=0? 1 : shape[n-1];
				}
				fn(fn, n-1, sh+w*m, w*(2+shape[n-1]));
			} else {
				if (k) {
					node.lnk.push_back(lctr[sh]);
				} k++;
			}
		}; fn(fn, nd);
	}

}
