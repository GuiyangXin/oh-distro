classdef DRCTerrainMap < RigidBodyTerrain

  methods
    function obj = DRCTerrainMap(is_robot,options)

      if nargin < 1
        is_robot = false;
      end

      if is_robot
        private_channel = true;
      else
        private_channel = false;
      end

      if nargin < 2
        options = struct();
      else
        typecheck(options,'struct');
      end

      if isfield(options,'name');
        typecheck(options.name,'char');
      else
        options.name = '';
      end

      if isfield(options,'status_code')
        typecheck(options.status_code, 'numeric');
      else
        options.status_code = 3;
      end

      if isfield(options,'raw');
        typecheck(options.raw,'logical');
      else
        options.raw = false;
      end
      obj.raw = options.raw;

      if isfield(options,'fill')
          typecheck(options.fill,'logical');
      else
          options.fill= false;
      end

      if isfield(options,'normal_radius')
        % Radius (in pixels) around each point to use for smoothing normals
        typecheck(options.normal_radius, 'numeric');
      else
        options.normal_radius = 1;
      end

      if isfield(options,'normal_method')
        typecheck(options.normal_method, 'char');
      else
        options.normal_method = 'leastsquares';
      end

      if isfield(options, 'auto_request')
        typecheck(options.auto_request, 'logical');
      else
        options.auto_request = false;
      end

      if (private_channel)
          obj.map_handle = HeightMapHandle(@HeightMapWrapper,'true');
      else
          obj.map_handle = HeightMapHandle(@HeightMapWrapper,'false');
      end

      obj.map_handle.setFillMissing(options.fill);
      obj.map_handle.setNormalRadius(options.normal_radius);
      obj.map_handle.setNormalMethod(options.normal_method);
    end

    function [z,normal] = getHeight(obj,xy)
      z = ones(1,size(xy,2));  normal=repmat([0;0;1],1,size(xy,2));
      [p,normal] = obj.map_handle.getClosest([xy;0*xy(1,:)]);
      z=p(3,:);
      if ~obj.raw
        if any(isnan(z))  % temporary hack because the robot is initialized without knowing the ground under it's feet
          if isempty(obj.backup_terrain)
            error('Received NaNs from heightmap, but no backup terrain is set');
          else
%             disp('using backup kinematic terrain map');
            nan_mask = isnan(z);
            [z(nan_mask), normal(:,nan_mask)] = obj.backup_terrain.getHeight(xy(:,nan_mask));
          end
        end
      end
    end

    function obj = setMapMode(obj,mode)
      obj.map_handle.setMapMode(mode);
    end

    function writeWRL(obj,fptr)
      error('not implemented yet, but could be done using the getAsMesh() interface');
    end

    function feas_check = getStepFeasibilityChecker(obj, contact_pts, options)
      % Return a function which can be called on a list of [x;y;yaw] points and which returns a vector of length size(xyyaw, 2) whose entries are 1 for each point which is OK for stepping and 0 for each point which is unsafe.
      if nargin < 3; options = struct(); end
      if ~isfield(options, 'resample'); options.resample = 2; end
      if ~isfield(options, 'debug'); options.debug = false; end
      if ~isfield(options, 'n_bins'); options.n_bins = 16; end

      %% Get the full heightmap from the map wrapper and a transform to world coordinate and then interpolate the heightmap and update the transform
      [heights, px2world] = obj.map_handle.getRawHeights();
      px2world(1,end) = px2world(1,end) - sum(px2world(1,1:3)); % stupid matlab 1-indexing...
      px2world(2,end) = px2world(2,end) - sum(px2world(2,1:3));
      mag = 2^(options.resample-1);
      heights = interp2(heights, (options.resample-1));
      px2world = px2world * [1/mag 0 0 (1-1/mag); 0 1/mag 0 (1-1/mag ); 0 0 1 0; 0 0 0 1];

      world2px = inv(px2world);
      world2px_2x3 = world2px(1:2,[1,2,4]);

      %% Run simple edge detectors across the heightmap
      Q = imfilter(heights, [1, -1]) - 0.03 > 0;
      Q = Q | imfilter(heights, [-1, 1]) - 0.03 > 0;
      Q = Q | imfilter(heights, [1; -1]) - 0.03 > 0;
      Q = Q | imfilter(heights, [-1; 1]) - 0.03 > 0;
      Q(isnan(heights)) = 1;

      contact_pts_px = world2px_2x3 * [contact_pts(1:2,:); ones(1,size(contact_pts,2))];
      contact_pts_px = bsxfun(@minus, contact_pts_px, mean(contact_pts_px, 2));
      contact_pts_px = contact_pts_px - 1 * sign(contact_pts_px); % shrink by 1px to adjust for edge effects

      n_bins = options.n_bins;
      expansions = repmat({Q}, n_bins, 1);
      dtheta = pi / n_bins;
      function bin = findBin(theta)
        bin = mod(round(theta/(2*dtheta)), n_bins) + 1;
      end
      function theta = findTheta(bin)
        theta = (bin - 1) * 2 * dtheta;
      end

      for j = 1:n_bins
        expansions{j} = filter2(obj.makeDomain(contact_pts_px, findTheta(j), dtheta), Q) > 0;
      end

      [px_X, px_Y] = meshgrid(1:size(heights, 2), 1:size(heights, 1));
      function feas = feas_check_fcn(xyy)
        bins = findBin(xyy(3,:));
        px = world2px_2x3 * [xyy(1:2,:); ones(1,size(xyy,2))];
        feas = zeros(1,size(xyy,2));
        for j = 1:n_bins
          mask = bins == j;
          near_px_x = min(max(1, round(px(1,mask))), size(heights, 2));
          near_px_y = min(max(1, round(px(2,mask))), size(heights, 1));
          ndx = (near_px_x-1) * size(heights,1) + near_px_y;
          feas(mask) = ~expansions{j}(ndx);
        end
      end
      feas_check = @feas_check_fcn;

      if options.debug
        px2world_2x3 = px2world(1:2, [1,2,4]);
        world_xy = px2world_2x3 * [reshape(px_X, 1, []); reshape(px_Y, 1, []); ones(1, size(px_X, 1) * size(px_X, 2))];
        Infeas = expansions{findBin(-pi/8)};
        colors = zeros(length(reshape(Infeas,[],1)), 3);
        colors(reshape(Infeas, [], 1) == 1, :) = repmat([1 0 0], length(find(reshape(Infeas, [], 1) == 1)), 1);
        colors(reshape(Q, [], 1) == 0, :) = repmat([1 1 0], length(find(reshape(Q, [], 1) == 0)), 1);
        colors(reshape(Infeas, [], 1) == 0, :) = repmat([0 1 0], length(find(reshape(Infeas, [], 1) == 0)), 1);
        lcmgl = drake.util.BotLCMGLClient(lcm.lcm.LCM.getSingleton(), 'terrain_safety');
        lcmgl.glPointSize(5);
        ht = reshape(heights, [], 1);
        possible_colors = {[1,0,0], [1,1,0], [0,1,0]};
        for j = 1:3
          lcmgl.glColor3f(possible_colors{j}(1),possible_colors{j}(2),possible_colors{j}(3));
          lcmgl.glBegin(lcmgl.LCMGL_POINTS);
          for k = 1:size(colors,1)
            if all(colors(k,:) == possible_colors{j});
              lcmgl.glVertex3f(world_xy(1,k), world_xy(2,k), ht(k));
            end
          end
          lcmgl.glEnd();
        end
        lcmgl.switchBuffers();
      end
    end

    function obj = setBackupTerrain(obj, biped, q0)
      obj.backup_terrain = KinematicTerrainMap(biped, q0, false);
    end

  end

  methods (Static=true)
    function domain = makeDomain(contact_pts_px, theta, dtheta)
      n_pts = size(contact_pts_px, 2);
      n_steps = 4;
      expanded_contacts_px = zeros(2,n_steps+size(contact_pts_px,2));
      expanded_contacts_px(:,1:n_pts) = rotmat(theta) * contact_pts_px;
      thetas = linspace(-dtheta, dtheta, n_steps);
      for j = 1:n_steps
        R = rotmat(thetas(j) + theta);
        expanded_contacts_px(:,n_pts*(j)+1:n_pts*(j+1)) = R * contact_pts_px;
      end
      [A, b] = poly2lincon(expanded_contacts_px(1,:), expanded_contacts_px(2,:));
      ma = max(expanded_contacts_px, [], 2);
      mi = min(expanded_contacts_px, [], 2);
      domain = false(ceil(ma(2) - mi(2)), ceil(ma(1) - mi(1)));
      x = (1:size(domain, 2)) - (size(domain, 2)/2 + 0.5);
      y = -((1:size(domain, 1)) - (size(domain, 1)/2 + 0.5));
      [X, Y] = meshgrid(x,y);
      inpoly = all(bsxfun(@minus, A * [reshape(X,1,[]); reshape(Y,1,[])], b) <= 0);
      domain(inpoly) = 1;
    end

    function [Expanded, dx, dy] = expandInfeasibility(Infeas, foot_radius_px)
      % Configuration-space expansion of infeasible region
      % @retval Expanded a matrix of the same size as Infeas. A point for which Expanded(j,k) > 0 is unsafe for stepping.
      % @retval dx gradient of infeasibility in x direction. Moving downhill means moving towards a safe step

      Infeas(Infeas > 0) = 1;

      %% Construct a circular domain corresponding to foot_radius
      domain = zeros(ceil(2 * foot_radius_px), ceil(2 * foot_radius_px));
      x = (1:size(domain, 2)) - (size(domain, 2)/2 + 0.5);
      y = (1:size(domain, 1)) - (size(domain, 1)/2 + 0.5);
      [X, Y] = meshgrid(x,y);
      d = sqrt(X.^2 + Y.^2);
      domain(d < foot_radius_px) = 1 - d(d < foot_radius_px) / foot_radius_px;

      %% A point is infeasible if any point within the domain centered on that point is infeasible (this is just configuration space planning)
      Expanded = imfilter(Infeas, domain);
    end
  end

  properties
    map_handle = [];
    backup_terrain = [];
    raw = false; % hackish for footstep planner---probably going away
  end
end
