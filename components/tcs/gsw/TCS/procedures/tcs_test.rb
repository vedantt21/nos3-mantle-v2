require 'cosmos'
require 'cosmos/script'
require 'tcs_lib.rb'

class TCS_Functional_Test < Cosmos::Test
  def setup
    safe_tcs()
  end

  def test_application
      start("tests/tcs_app_test.rb")
  end

  def test_device
    start("tests/tcs_device_test.rb")
  end

  def teardown
    safe_tcs()
  end
end

class TCS_Automated_Scenario_Test < Cosmos::Test
  def setup 
    safe_tcs()
  end

  def test_AST
      start("tests/tcs_ast_test.rb")
  end

  def teardown
    safe_tcs()
  end
end

class Tcs_Test < Cosmos::TestSuite
  def initialize
      super()
      add_test('TCS_Functional_Test')
      add_test('TCS_Automated_Scenario_Test')
  end

  def setup
    safe_tcs()
  end
  
  def teardown
    safe_tcs()
  end
end
