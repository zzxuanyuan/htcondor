# cmd_cs_control.rb: Cluster Suite control
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

module Mrg
  module Grid
    module Config
      module Shell
        module CSControlOps
          def cmd_args
            {:create=>[:name, :spool, :server, :export, :nodes],
             :delete=>[:name],
             :add_node=>[:name, :nodes],
             :remove_node=>[:name, :nodes]}
          end

          def convert(sym)
            sym.to_s.gsub(/(.+)s$/,'\1').upcase
          end

          def modify_group_membership
            if @action == :create
              prefix = ["add", "node"]
            else
              prefix = @action.to_s.split('_')
            end
            prefix.collect! {|n| n.capitalize}

            if not prefix[0].include?("Remove")
              # Add missing nodes
              store.checkNodeValidity(@options[:nodes]).each do |node|
                store.addNode(node)
              end
            end

            Mrg::Grid::Config::Shell.const_get("#{prefix.to_s}Group").new(store, "").main([@group] + @options[:nodes])
          end

          def args_for_help(list)
            list.collect{|n| n != :nodes ? "#{convert(n)}=VALUE" : "#{convert(n)} [...]"}.join(" ")
          end

          def init_option_parser
            @options = {}
            OptionParser.new do |opts|

              opts.banner = "Usage:\n"
              cmd_args.keys.each do |key|
                opts.banner += "  #{File.basename($0)} #{key} #{args_for_help(cmd_args[key])}\n"
              end
        
              opts.on("-h", "--help", "displays this message") do
                puts @oparser
                exit
              end

#              opts.separator "Broker Options:"
#              opts.on("-b", "--broker HOSTNAME", "The qpid broker hostname used by the configuration store") do |h|
#                ws_args << ["-H", h]
#              end

#              opts.on("-p", "--port NUM", Integer, "The qpid broker port used by the configuration store") do |p|
#                ws_args << ["-p", p]
#              end

#              opts.on("-U", "--user NAME", "The username used to authenticate with the qpid broker") do |n|
#                ws_args << ["-U", n]
#              end

#              opts.on("-P", "--password PASS", "The password used to authenticate with the qpid broker") do |p|
#                ws_args << ["-P", p]
#              end

#              opts.on("-M", "--auth-mechanism PASS", mechanisms, "Authentication mechanism (#{mechanisms.join(", ")})") do |m|
#                ws_args << ["-M", m]
#              end

#              opts.separator ""
#              opts.separator "Configuration Options:"
              opts.on("-c", "--create", "Create missing entities in the store") do
                @options[:create] = true
              end

              opts.on("--riccipassword PASS", "The ricci user password") do |p|
                @options[:password] = p
              end

              opts.on("-e", "--expire NUM", "Length of time to forget a restart") do |num|
                @options[:expire] = num
              end

              opts.on("-m", "--max-restarts NUM", "Maximum number of restarts before relocation (default 3)") do |num|
                @options[:max_restarts] = num
              end

              opts.on("--new-cluster", "Create a new cluster even it one already exists") do
                @options[:new_cluster] = true
              end
              opts.on("-n", "--no-store", "Only configure the cluster, don't update the store") do
                @options[:cluster_only] = true
              end

              opts.on("-s", "--store-only", "Don't configure the cluster, just update the store") do
                @options[:wallaby_only] = true
              end
            end
          end
        
          def parse_args(*args)
            @action = self.class.opname.split("-", 3)[2].gsub(/-/, '_').to_sym
            valid_args = cmd_args[@action]
            if args.length < valid_args.length
              exit!(1, "Too few arguments")
            end

            args.each do |arg|
              pair = arg.split('=', 2)
              if pair.length > 1
                if not valid_args.include?(pair[0].to_sym)
                  exit!(1, "Argument #{pair[0]} is not valid for action #{@action}")
                end
                @options[pair[0].to_sym] = pair[1]
              else
                @options.has_key?(:nodes) ? @options[:nodes].push(pair[0]) : @options[:nodes] = [pair[0]]
              end
            end

            # Determine if -i is needed
            `ccs -h localhost -i`
            ignore = "-i" if $?.exitstatus == 0
            @ccs = "ccs #{ignore} -h localhost"

            @domain = "Schedd #{@options[:name]} Failover Domain"
            @password = "--password #{@options[:password]}" if @options.has_key?(:password)
            @service = "HA Schedd #{@options[:name]}"
            @subsys = @options[:name]
            @group = "HAScheduler#{@subsys}"
          end

          def self.included(receiver)
            if receiver.respond_to?(:register_callback)
              receiver.register_callback :after_option_parsing, :parse_args
            end
          end

          def modify_ccs_nodes
            cmd = (@action.to_s.include?("remove") ? "rm" : "add")
            @options[:nodes].each do |node|
              `#{@ccs} --#{cmd}node #{node} #{@password}` if cmd == "add"
              `#{@ccs} --#{cmd}failoverdomainnode '#{@domain}' #{node} #{@password}`
            end
          end
        end

        module NodeOps
          def act
          end
        end

        class CSControlCreate < ::Mrg::Grid::Config::Shell::Command
          include CSControlOps

          def self.opname
            "cs-control-create"
          end
        
          def self.description
            "Create an HA Schedd cluster configuration"
          end
        
          def act
            if not @options.has_key?(:wallaby_only)
              # Cluster Suite config
              restarts = @options[:max_restarts] ? @options[:restarts] : 3
              expire = @options[:expire] ? @options[:expire] : 300

              if (not File.exists?("/etc/cluster/cluster.conf")) || @options.has_key?(:new_cluster)
                `#{@ccs} --createcluster "HA Schedds" #{@password}`
                if $?.exitstatus != 0
                  puts "Failed to create the cluster.  Try supplying a ricci password with --riccipassword"
                  exit!(1)
                end
              end

              # Handle nodes
              `#{@ccs} --addfailoverdomain "#{@domain}" restricted #{@password}`
              modify_ccs_nodes

              # Handle service
              `#{@ccs} --addservice "#{@service}" domain="#{@domain}" autostart=1 recovery=restart max_restarts=#{restarts} restart_expire_time=#{expire} #{@password}`

              # Handle subservices
              `#{@ccs} --addsubservice "#{@service}" netfs name="Job Queue" mountpoint="#{@options[:spool]}" host="#{@options[:server]}" export="#{@options[:export]}" options="rw,soft" force_unmount="on" #{@password}`
              `#{@ccs} --addsubservice "#{@service}" condor_schedd name="#{@options[:name]}" #{@password}`
            end

            if not @options.has_key?(:cluster_only)
              # Store config
              if @options.has_key?(:wallaby_only) and get_from_ccs
              end

              # Add the Group if it doesn't exist
              if store.checkGroupValidity([@group]) != []
                store.addExplicitGroup(@group)
              end

              # Add Parameters if they don't exist
              params = {@subsys=>"$(SCHEDD)", "#{@subsys}.USE_PROCD"=>"False",
                     "SCHEDD.#{@subsys}.SCHEDD_NAME"=>"ha-schedd-#{@subsys}@",
                     "SCHEDD.#{@subsys}.SPOOL"=>@options[:spool],
                     "SCHEDD.#{@subsys}.HISTORY"=>"$(SCHEDD.#{@subsys}.SPOOL)",
                     "SCHEDD.#{@subsys}.SCHEDD_LOG"=>"$(LOG)/SchedLog-#{@subsys}.log",
                     "SCHEDD.#{@subsys}.SCHEDD_ADDRESS_FILE"=>"$(LOG)/.schedd-#{@subsys}-address"
              }
              store.checkParameterValidity(params.keys).each do |pname|
                # Add missing params
                pargs = ["--needs-restart", "yes", "--kind", get_kind(pname), "--description", "Created for HA Schedd #{@subsys}", pname]
                Mrg::Grid::Config::Shell::AddParam.new(store, "").main(pargs)
              end

              param_list = []
              params.each_pair do |n, v|
                param_list << "#{n}=#{v}"
              end
              Mrg::Grid::Config::Shell::AddGroupParam.new(store, "").main([@group] + param_list)

#              # Add group membership to nodes
#              store.checkNodeValidity(@options[:nodes]).each do |node|
#                store.addNode(node)
#              end
              modify_group_membership
#              Mrg::Grid::Config::Shell::AddNodeGroup.new(store, "").main([@group] + @options[:nodes])
            end

            return 0
          end

          def get_kind(n)
            n.include?("USE_PROCD") ? "Boolean" : "String"
          end
        end

        class CSControlDelete < ::Mrg::Grid::Config::Shell::Command
          include CSControlOps

          def self.opname
            "cs-control-delete"
          end
        
          def self.description
            "Delete an HA Schedd cluster configuration"
          end
        
          def act
            if @options.has_key?(:nodes)
              puts "Warning: nodes are ignored for removal operations"
            end

            if not @options.has_key?(:wallaby_only)
              # Cluster Suite config
              `#{@ccs} --rmsubservice "#{@service}" condor_schedd #{@password}`
              `#{@ccs} --rmsubservice "#{@service}" netfs #{@password}`
              `#{@ccs} --rmservice "#{@service}" #{@password}`
              `#{@ccs} --rmfailoverdomain "#{@domain}" #{@password}`
            end

            if not @options.has_key?(:cluster_only)
              # Store config

              # Remove the Group if it exists
              if store.checkGroupValidity([@group]) == []
                store.removeGroup(@group)
              end
            end
            return 0
          end
        end

        class CSControlAddNode < ::Mrg::Grid::Config::Shell::Command
          include CSControlOps

          def self.opname
            "cs-control-add-node"
          end
        
          def self.description
            "Add a node to an existing HA Schedd cluster configuration"
          end
        
          def act
            if not @options.has_key?(:wallaby_only)
              modify_ccs_nodes
            end

            if not @options.has_key?(:cluster_only)
              # Store config

              # Add the Group if it doesn't exist
              if store.checkGroupValidity([@group]) != []
                store.addExplicitGroup(@group)
              end

#              # Add group membership to nodes
#              store.checkNodeValidity(@options[:nodes]).each do |node|
#                store.addNode(node)
#              end
              modify_group_membership
#              Mrg::Grid::Config::Shell::AddNodeGroup.new(store, "").main([@group] + @options[:nodes])
            end
            return 0
          end
        end

        class CSControlRemoveNode < ::Mrg::Grid::Config::Shell::Command
          include CSControlOps

          def self.opname
            "cs-control-remove-node"
          end
        
          def self.description
            "Remove a node from an existing HA Schedd cluster configuration"
          end
        
          def act
            if not @options.has_key?(:wallaby_only)
              modify_ccs_nodes
            end

            if not @options.has_key?(:cluster_only)
              # Store config

              if store.checkGroupValidity([@group]) == []
                modify_group_membership
#                Mrg::Grid::Config::Shell::RemoveNodeGroup.new(store, "").main([@group] + @options[:nodes])
              end
            end
            return 0
          end
        end
      end
    end
  end
end
