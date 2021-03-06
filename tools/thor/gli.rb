require File.dirname(__FILE__) + '/base.rb'
require File.dirname(__FILE__) + '/git.rb'

module Mmdai

class Gli < Thor
  include Build::Base
  include VCS::Git

  desc "build", "build GLI (doesn't build actually)"
  def build
    checkout
  end

  desc "clean", "delete built GLM libraries (do nothing)"
  def clean
  end

protected
  def get_uri
    "https://github.com/g-truc/gli.git"
  end

  def get_directory_name
    "gli-src"
  end

  def get_tag_name
    "0.4.1.0"
  end

end

end

